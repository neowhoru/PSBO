#pragma once

#include <malloc.h>
#include <stddef.h>
#include <cstddef>

#ifndef WIN32
#ifdef _WIN32
#define WIN32
#endif
#endif

void init_platform_specific(char const * const title);
bool pp_kbhit() NOEXCEPT;
int pp_getch() NOEXCEPT;
unsigned get_tick() NOEXCEPT;
void set_console_color(int color) NOEXCEPT;
void clear_screen();
extern const char * const ansi_console_color_codes[16];
extern unsigned console_height;
extern unsigned console_width;

void syslog_info(std::string const & str);
void syslog_warning(std::string const & str);
void syslog_error(std::string const & str);

