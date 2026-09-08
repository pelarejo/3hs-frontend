/* This file is part of 3hs
 * Copyright (C) 2021-2026 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "circular_queue.hh"
#include "httpclient.hh"
#include "settings.hh"
#include "install.hh"
#include "thread.hh"
#include "error.hh"
#include "panic.hh"
#include "log.hh"
#include "mng.hh"

#include <3ds.h>

namespace ui
{
	void scan_keys();
	u32 kDown();
	u32 kHeld();
}

using expandable_binary_data_type = std::basic_string<u8>;

namespace install { Handle active_cia_handle = CIA_HANDLE_INVALID; }
using install::active_cia_handle;

/* defined in file_fwd.cc */
Result install_forwarder(u8 *data, size_t len);

enum class ThreadState {
	Installing,
	Timeout,
	Abort,
	Finished,
};

struct thread_data {
	http::ResumableDownload *downloader;
	get_url_func *get_url;
	ctr::Event thread_to_ui_event;
	ctr::Event ui_to_thread_event;
	Result last_result;
	ThreadState state;

	thread_data() :
		thread_to_ui_event(ctr::Event::ResetType::Oneshot),
		ui_to_thread_event(ctr::Event::ResetType::Oneshot)
	{}
};

static void install_generic_thread(thread_data *data)
{
	std::string url;
	Result res;

	data->downloader->set_notify_event(&data->thread_to_ui_event);

	if(!ISET_RESUME_DOWNLOADS)
	{
		res = (*data->get_url)(url);
		if(R_FAILED(res))
		{
			elog("failed to fetch URL: %08lX", res);
			goto out;
		}
		data->downloader->set_target(url, HTTPC_METHOD_GET);
		res = data->downloader->execute_once();
	}
	else
	{
		while(data->state == ThreadState::Installing)
		{
			res = (*data->get_url)(url);
			if(R_SUCCEEDED(res))
			{
				data->downloader->set_target(url, HTTPC_METHOD_GET);
				res = data->downloader->execute_once();
				if(R_FAILED(res))
					elog("Failed to execute_once: %08lX", res);
			}
			else elog("Failed to fetch URL: %08lX", res);

			if(R_MODULE(res) == RM_HTTP)
			{
				/* there has probably been a timeout, we have to
				 * show this to the user and continue in due time */
				data->last_result = res;
				data->state = ThreadState::Timeout;
				data->thread_to_ui_event.signal();
				data->ui_to_thread_event.wait();
				/* the ui thread will set data->state to ThreadState::Insalling
				 * or ThreadState::Abort depending on what the user wants */
				continue;
			}

			/* Two cases:
			 *  - install succeeded: result is 0 and the above if is not reached; we want to break
			 *  - install failed (non-http error): result is not 0 and the above is not reached; we want to break */
			break;
		}
		if(data->state == ThreadState::Abort)
			res = APPERR_CANCELLED;
	}

out:
	data->last_result = res;
	data->state = ThreadState::Finished;
	data->thread_to_ui_event.signal();
	return;
}

static Result install_generic(http::ResumableDownload *downloader, get_url_func *get_url, prog_func *on_progress = nullptr)
{
	thread_data data;
	data.state = ThreadState::Installing;
	data.downloader = downloader;
	data.get_url = get_url;
	data.last_result = 0;

	ctr::thread<thread_data *> thread(install_generic_thread, 1, (thread_data *)&data);
	for(;;)
	{
		data.thread_to_ui_event.wait();
		/* te installation process is finished; we can exit this loop */
		if(data.state == ThreadState::Finished)
			break;
		/* the install thread wants us to display a timeout to the user */
		else if(data.state == ThreadState::Timeout)
		{
			if(ui::timeoutscreen(data.last_result, 10)) data.state = ThreadState::Abort;
			else                                        data.state = ThreadState::Installing;
			if (on_progress) (*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
			data.ui_to_thread_event.signal();
		}
		/* the install thread wants us to simply update the progress */
		else if(on_progress && data.state == ThreadState::Installing)
			(*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
		/* now we check if the user wants to abort */
		/* TODO: Ideally we'd subscribe to an event that tells us when
		 *       a button is pressed... */
		ui::scan_keys();
		/* The abort() will cause the ResumableDownload to return APPERR_CANCELLED, which in
		 * turn is futher thrown down more to cause an abort when ready to cancel */
		if(!aptMainLoop() || ((ui::kDown() | ui::kHeld()) & (KEY_B | KEY_START)))
			downloader->abort();
	}

	thread.join();
	return data.last_result;
}

static bool have_enough_space(ctr::InstallDestination dest, u32 min_space)
{
	u64 freeSpace;
	Result res;
	if(R_FAILED(res = ctr::mng::get_free_space(dest, &freeSpace)))
		return false;
	return min_space <= freeSpace;
}

void install::global_abort()
{
	if(active_cia_handle != CIA_HANDLE_INVALID)
	{
		AM_CancelCIAInstall(active_cia_handle);
		svcCloseHandle(active_cia_handle);
		active_cia_handle = CIA_HANDLE_INVALID;
	}
}

struct TitleInformation {
	ctr::title_id tid;
	u64 file_size;
	bool isKTR;
	u16 version;
};

struct Chunk {
	void *ptr;
	u32 size;
	u32 offset;
};

static Result install_generic_cia(get_url_func *get_url, prog_func *on_progress, bool reinstallable, const TitleInformation& info, bool hsapi_enabled, bool do_ver_check, bool dev_auth)
{
	panic_assert(active_cia_handle == CIA_HANDLE_INVALID, "May only install one CIA at a time");

	/* firstly we perform the prerequisite checks */
	FS_MediaType dest = info.tid.installation_media();
	Result res;

	/* check if the title is already installed and perhaps reinstall */
	bool has_ticket = ctr::mng::ticket_exists(info.tid);
	bool has_title  = ctr::mng::title_exists(info.tid); /* we do not need to check on gamecart here because we can't reinstall on gamecart (duh) */
	if(has_ticket && !has_title)
	{
		ctr::mng::delete_ticket(info.tid.raw);
		/* reload dbs */
		AM_QueryAvailableExternalTitleDatabase(NULL);
	}
	AM_TitleEntry entry;
	/* only reinstall if we want to update */
	/* TODO: We should probably attempt to get this info from the CIA when the first chunk downloads */
	if(has_title && !(R_FAILED(ctr::mng::get_title_entry(info.tid.raw, entry)) || entry.version > info.version))
	{
		if(reinstallable || ISET_DEFAULT_REINSTALL)
		{
			FS_MediaType mydest;
			ctr::title_id mytid;
			if(R_FAILED(res = APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &mytid.raw, (u8 *) &mydest, nullptr, nullptr, nullptr)))
				return res;
			/* we can only delete titles that are not ourselves */
			if(envIsHomebrew() || mytid != info.tid || mydest != dest)
			{
				if(R_FAILED(res = ctr::mng::delete_title(info.tid.raw, ctr::mng::DeleteTitleFlag::DeleteTicket | ctr::mng::DeleteTitleFlag::CheckExist)))
					return res;
				/* reload dbs */
				AM_QueryAvailableExternalTitleDatabase(NULL);
			}
		}
		else return APPERR_NOREINSTALL;
	}

	/* check if the title is meant exclusively for the "new" series */
	if(info.isKTR && !ctr::mng::is_n3ds())
		return APPERR_NOSUPPORT;

	/* we can only start the CIA installation a bit later due to
	 * some checks requiring file size, which is gotten through
	 * downloader.on_total_size_try_get() */

	http::ResumableDownload downloader(hsapi_enabled, do_ver_check, dev_auth);
	Handle ciaHandle = CIA_HANDLE_INVALID;
	u32 written;

	CircularQueue<Chunk> chunks;
	LightEvent stop_am_thread;
	LightEvent_Init(&stop_am_thread, RESET_STICKY);
	Result am_res = 0;

	ctr::thread<> am_thread([&]() -> void {
		u32 written;

		while(!LightEvent_TryWait(&stop_am_thread))
		{
			const Chunk chunk = chunks.dequeue();

			if (!chunk.ptr) break;

			/* we don't need to add the FS_WRITE_FLUSH flag because AM just ignores write flags... */
			am_res = FSFILE_Write(ciaHandle, &written, chunk.offset, chunk.ptr, chunk.size, 0);
			free(chunk.ptr);

			/* if we failed to write the cia abort the downloader */
			if(R_FAILED(am_res))
			{
				downloader.abort();
				break;
			}

			if (info.file_size != -1ULL)
				(*on_progress)(chunk.offset + chunk.size, info.file_size);

		}
	}, 1);

	downloader.on_total_size_try_get([&]() -> Result {
		if(!downloader.maybe_total_size())
			return APPERR_NOSIZE;
		/* we do a little "do we have the required size" check before we do the actual work */
		ctr::InstallDestination dest = info.tid.detect_dest();
		if(!have_enough_space(dest, downloader.maybe_total_size()))
			return APPERR_NOSPACE;

		/* just here can we actually start installing */
		res = AM_StartCiaInstall(dest.to_mediatype(), &ciaHandle);
		if(R_FAILED(res)) ciaHandle = CIA_HANDLE_INVALID;
		active_cia_handle = ciaHandle;
		return res;
	});

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		/* first we copy the data into a chunk and push it into the queue */
		void *buf_copy = malloc(chunk_size);

		if (!buf_copy)
			return APPERR_OUT_OF_MEM;

		memcpy(buf_copy, downloader.data_buffer(), chunk_size);
		do {
			if (chunks.try_enqueue(Chunk { .ptr = buf_copy, .size = chunk_size, .offset = downloader.downloaded() })) {
				return 0;
			}

			svcSleepThread(0); /* yield just in case */
		} while (!downloader.is_exiting());

		free(buf_copy);
		return 0;
	});

	/*
		home menu polls AM, and if this is done while something is installing, it will crash.
		thus, we must disallow suspending while a title is installing
	*/
	bool homeAllowedRestore = aptIsHomeAllowed();
	aptSetHomeAllowed(false);

	res = install_generic(&downloader, get_url, info.file_size == -1ULL ? on_progress : nullptr);

	do {
		if (chunks.try_enqueue(Chunk{})) {
			break;
		}
	} while (!am_thread.join(0));

	if (R_FAILED(res) || R_FAILED(am_res)) {
		LightEvent_Signal(&stop_am_thread);
	}

	/* wait for thread exit */
	am_thread.join();

	/* clean up any leftover chunks */
	while (!chunks.empty())
		free(chunks.dequeue().ptr);

	ilog("install_generic returned %08lX", res);
	ilog("AM thread returned %08lX", am_res);
	if(R_FAILED(am_res))
		res = am_res;

	/* finalize install */
	if(ciaHandle != CIA_HANDLE_INVALID)
	{
		if(R_FAILED(res)) AM_CancelCIAInstall(ciaHandle);
		else              res = AM_FinishCiaInstall(ciaHandle);
		svcCloseHandle(ciaHandle);
	}
	active_cia_handle = CIA_HANDLE_INVALID;

	aptSetHomeAllowed(homeAllowedRestore);

	ilog("final return of install_generic_cia is %08lX", res);
	return res;
}

Result install::net_cia(get_url_func get_url, ctr::title_id tid, prog_func prog, bool reinstallable, bool hsapi_enabled, bool do_ver_check, bool dev_auth)
{
	/* net_cia: both hsapi and do_ver_check should be configurable, we may not be downloading from hs */
	return install_generic_cia(&get_url, &prog, reinstallable, { tid, -1ULL, false, 0 }, hsapi_enabled, do_ver_check, dev_auth);
}

Result install::hs_cia(const hsapi::Title& meta, prog_func prog, bool reinstallable)
{
	Result res;
	get_url_func get_url = [meta](std::string& ret) -> Result {
		return hsapi::get_download_link(ret, meta);
	};

	if(meta.flags & hsapi::TitleFlag::installer)
	{
		ilog("installing installer content");

		expandable_binary_data_type content;
		http::ResumableDownload downloader(true, true, true);

		downloader.on_total_size_try_get([&]() -> Result {
			if(!downloader.maybe_total_size())
				return APPERR_NOSIZE;
			/* file forwarders will always install to the SD card */
			if(!have_enough_space(ctr::InstallDestination::SDMC, downloader.maybe_total_size()))
				return APPERR_NOSPACE;
			content.reserve(downloader.maybe_total_size());
			return 0;
		});

		downloader.on_chunk([&](size_t chunk_size) -> Result {
			content.append(downloader.data_buffer(), chunk_size);
			return 0;
		});

		res = install_generic(&downloader, &get_url, &prog);
		if(R_FAILED(res)) return res;
		return install_forwarder((u8 *) content.c_str(), content.size());
	}
	else
	{
		res = install_generic_cia(&get_url, &prog, reinstallable,
			{ meta.tid, meta.size,
			  (meta.flags & hsapi::TitleFlag::is_ktr) || strncmp(meta.prod.c_str(), "KTR-", 4) == 0,
			  meta.version }, true, true, true); /* hs_cia: hsapi, do ver check, use dev auth */
	}
	return res;
}

