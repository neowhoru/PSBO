#pragma once

#include <string>
#include "string.h"
#include "common.h"

void init_config(std::vector<std::string> const & args);
void init_config(int argc, char * argv[]);

extern bool const debug_build;

void set_config(std::string const & key, std::string const & value);
void set_config_handler(std::string const & key, void(*func)(std::string const &, std::string const &), bool init_call = false);
#define config_to_global(name, v) set_config_handler((name), [](std::string const & key, std::string const & value) {convert(value, (v)); }, true);
#define config_to_global2(v) set_config_handler( #v , [](std::string const & key, std::string const & value) {convert(value, (v)); }, true);
std::string get_config(std::string const & key);

bool has_config(std::string const & key);
template <class T>
inline T get_config(std::string const & key)
{
	return convert<T>(get_config(key));
}
