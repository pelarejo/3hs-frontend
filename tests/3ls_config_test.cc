#include "3ls_config.hh"

#include <cassert>
#include <cstdio>
#include <fstream>

int main()
{
	const char *file = ".build-docker/3ls-config-test.bin";
	std::remove(file);
	assert(!ls_config::load(file));
	assert(ls_config::get().username.empty());
	assert(ls_config::get().token.empty());

	ls_config::Config expected;
	expected.server = "shop.local";
	expected.port = 8000;
	expected.username = "alice";
	expected.token = "not-printed-by-test";
	ls_config::set(expected);
	assert(ls_config::save(file));
	ls_config::set(ls_config::Config());
	assert(ls_config::load(file));
	assert(ls_config::get().server == expected.server);
	assert(ls_config::get().port == expected.port);
	assert(ls_config::get().username == expected.username);
	assert(ls_config::get().token == expected.token);
	assert(ls_config::base_url() == "http://shop.local:8000");
	assert(ls_config::nb_url() == "http://shop.local:8000/nbapi");

	// Saving again must truncate and replace the existing contents directly.
	expected.token = "replacement-token";
	ls_config::set(expected);
	assert(ls_config::save(file));
	ls_config::set(ls_config::Config());
	assert(ls_config::load(file));
	assert(ls_config::get().token == expected.token);

	// A truncated file is rejected and resets all runtime values to defaults.
	{
		std::ofstream truncated(file, std::ios::binary | std::ios::trunc);
		truncated << "3LSC\1";
	}
	assert(!ls_config::load(file));
	assert(ls_config::get().server.empty());
	assert(ls_config::get().port == 0);
	assert(ls_config::get().username.empty());
	assert(ls_config::get().token.empty());
	assert(ls_config::base_url().empty());

	// A structurally malformed file is rejected with the same default fallback.
	{
		const unsigned char malformed[] = {
			'3', 'L', 'S', 'C', 1, 0, 0x40, 0x1f, 2, 0, 'x'
		};
		std::ofstream config(file, std::ios::binary | std::ios::trunc);
		config.write((const char *)malformed, sizeof(malformed));
	}
	ls_config::set(expected);
	assert(!ls_config::load(file));
	assert(ls_config::get().server.empty());
	assert(ls_config::get().port == 0);
	assert(ls_config::get().username.empty());
	assert(ls_config::get().token.empty());

	{
		const unsigned char unsupported[] = {
			'3', 'L', 'S', 'C', 2, 0, 0x40, 0x1f, 0, 0, 0, 0, 0, 0
		};
		std::ofstream config(file, std::ios::binary | std::ios::trunc);
		config.write((const char *)unsupported, sizeof(unsupported));
	}
	ls_config::set(expected);
	assert(!ls_config::load(file));
	assert(ls_config::get().server.empty());
	assert(ls_config::get().port == 0);
	assert(ls_config::get().username.empty());
	assert(ls_config::get().token.empty());
	assert(ls_config::base_url().empty());
	std::remove(file);
}
