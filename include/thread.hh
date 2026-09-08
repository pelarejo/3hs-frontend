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

#ifndef inc_thread_hh
#define inc_thread_hh

#include <functional>
#include <3ds.h>
#include "panic.hh"


namespace ctr
{
	template <typename ... Ts>
	class thread
	{
	public:
		/* exit the current thread */
		static void exit()
		{
			threadExit(0);
		}

		/* create a new thread */
		thread(std::function<void(Ts...)> cb, int prioAddition, Ts&& ... args)
		{
			this->func = [cb, &args...] () -> void { cb(std::forward<Ts>(args)...); };

			s32 prio = 0;
			svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

			this->threadobj = threadCreate(&thread::_entrypoint, this, 64 * 1024, prio - prioAddition, -2, false);
			panic_assert(this->threadobj != nullptr, "failed to create thread");
		}

		~thread()
		{
			this->join();
		}

		/* wait for the thread to finish */
		/* return: true if dead, false if still running */
		bool join(u64 timeout_ns = U64_MAX)
		{
			if (!this->threadobj)
				return true; /* already dead */

			Result res = threadJoin(this->threadobj, timeout_ns);

			if (R_SUCCEEDED(res) && R_DESCRIPTION(res) != RD_TIMEOUT) {
				threadFree(this->threadobj);
				this->threadobj = nullptr;
				return true;
				/* thread is dead */
			}

			return false; /* thread did not exit even after waiting timeout_ns */
		}

	private:
		static void _entrypoint(void *arg)
		{
			reinterpret_cast<thread *>(arg)->func();
		}

		Thread threadobj;
		std::function<void()> func;

	};
}

#endif

