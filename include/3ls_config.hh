#ifndef inc_3ls_config_hh
#define inc_3ls_config_hh

#include <cstdint>
#include <string>

namespace ls_config
{
	struct Config
	{
		std::string server;
		uint16_t port = 0;
		std::string username;
		std::string token;
	};

	const char *path();
	const Config& get();
	void set(const Config& config);
	bool load(const char *config_path = nullptr);
	bool save(const char *config_path = nullptr);

	std::string base_url();
	std::string nb_url();
	std::string update_url();

	void show_editor();
}

#endif
