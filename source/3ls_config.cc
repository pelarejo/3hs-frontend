#include "3ls_config.hh"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

#define CONFIG_PATH "/3ds/3ls/server-config"

namespace
{
	const unsigned char magic[4] = { '3', 'L', 'S', 'C' };
	const uint16_t format_version = 1;
	ls_config::Config current;

	void append_u16(std::vector<unsigned char>& data, uint16_t value)
	{
		data.push_back(value & 0xff);
		data.push_back(value >> 8);
	}

	bool read_u16(const std::vector<unsigned char>& data, size_t& offset, uint16_t& value)
	{
		if(offset + 2 > data.size()) return false;
		value = data[offset] | (data[offset + 1] << 8);
		offset += 2;
		return true;
	}

	bool append_string(std::vector<unsigned char>& data, const std::string& value)
	{
		if(value.size() > 0xffff) return false;
		append_u16(data, value.size());
		data.insert(data.end(), value.begin(), value.end());
		return true;
	}

	bool read_string(const std::vector<unsigned char>& data, size_t& offset, std::string& value)
	{
		uint16_t size;
		if(!read_u16(data, offset, size) || offset + size > data.size()) return false;
		value.assign((const char *)&data[offset], size);
		offset += size;
		return true;
	}

	bool load_file(const char *path, ls_config::Config& parsed)
	{
		FILE *file = fopen(path, "rb");
		if(!file) return false;
		bool valid = false;
		long file_size = -1;
		if(fseek(file, 0, SEEK_END) != 0) goto out;
		file_size = ftell(file);
		if(file_size < 0 || file_size > 0x40008 || fseek(file, 0, SEEK_SET) != 0) goto out;
		{
			std::vector<unsigned char> data(file_size);
			if(file_size && fread(data.data(), file_size, 1, file) != 1) goto out;
			if(data.size() < 8 || memcmp(data.data(), magic, sizeof(magic)) != 0) goto out;
			size_t offset = 4;
			uint16_t version;
			if(!read_u16(data, offset, version) || version != format_version) goto out;
			if(!read_u16(data, offset, parsed.port)) goto out;
			if(!read_string(data, offset, parsed.server)) goto out;
			if(!read_string(data, offset, parsed.username)) goto out;
			if(!read_string(data, offset, parsed.token)) goto out;
			if(offset != data.size()) goto out;
			valid = true;
		}
	out:
		fclose(file);
		return valid;
	}

}

const char *ls_config::path() { return CONFIG_PATH; }
const ls_config::Config& ls_config::get() { return current; }
void ls_config::set(const ls_config::Config& config) { current = config; }

bool ls_config::load(const char *config_path)
{
	current = Config();
	const char *destination = config_path ? config_path : CONFIG_PATH;
	Config parsed;
	if(!load_file(destination, parsed)) return false;
	current = parsed;
	return true;
}

bool ls_config::save(const char *config_path)
{
	const char *destination = config_path ? config_path : CONFIG_PATH;
	std::vector<unsigned char> data(magic, magic + sizeof(magic));
	append_u16(data, format_version);
	append_u16(data, current.port);
	if(!append_string(data, current.server) || !append_string(data, current.username)
		|| !append_string(data, current.token)) return false;
	if(!config_path) { mkdir("/3ds", 0777); mkdir("/3ds/3ls", 0777); }
	FILE *file = fopen(destination, "wb");
	if(!file) return false;
	bool written = fwrite(data.data(), data.size(), 1, file) == 1;
	written = fflush(file) == 0 && written;
	written = fclose(file) == 0 && written;
	return written;
}

std::string ls_config::base_url()
{
	if(current.server.empty() || current.port == 0) return "";
	return "http://" + current.server + ":" + std::to_string(current.port);
}
std::string ls_config::nb_url() { return base_url() + "/nbapi"; }
std::string ls_config::update_url() { return base_url() + "/update"; }
