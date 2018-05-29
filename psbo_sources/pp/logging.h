#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <atomic>
#include "binarystream.h"

#define ELB dwn::LIGHT_RED << "Error " << dwn::remove_path(__FILE__) << ":" << __LINE__ << ": "


class string;
enum console_color_t { BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, ORANGE, GREY, DARK_GREY, LIGHT_BLUE, LIGHT_GREEN, LIGHT_CYAN, LIGHT_RED, PINK, YELLOW, WHITE };
struct endl_t {};

void _throw_simple_error(char const * const file, signed line, char const * const func, char const * const message);

extern thread_local console_color_t default_color;
extern console_color_t edit_color;

class logger_t;

namespace detail
{
	template <class T, bool conv = std::is_convertible<T, std::streamsize>::value>
	struct log_set_width
	{
		static inline void set(std::ostringstream & oss, T const & value)
		{
			if (value >= 0)
				oss << std::setw((std::streamsize)value);
			else
				oss << std::left << std::setw(-(std::streamsize)value);
		}
	};
	template <class T>
	struct log_set_width<T, false>
	{
		static inline void set(std::ostringstream & oss, T const & value)
		{}
	};

	template <class T, bool conv = std::is_convertible<T, std::streamsize>::value>
	struct log_set_precision
	{
		static inline void set(std::ostringstream & oss, T const & value)
		{
			if (value >= 0)
				oss << std::setprecision((std::streamsize)value);
		}
	};
	template <class T>
	struct log_set_precision<T, false>
	{
		static inline void set(std::ostringstream & oss, T const & value)
		{}
	};
};

class logger_session
{
	friend class logger_t;
	std::ostringstream ss;
	logger_t & l;
	logger_session(const logger_session &) = delete;
	void operator = (const logger_session &) = delete;

	logger_session(logger_t & l);

	template <class T>
	logger_session(T const & data, logger_t & l) NOEXCEPT
		: l(l)
	{
		try
		{
			*this << data;
		}
		catch (std::exception & e)
		{
			if (e.what())
				std::cerr << "Error while logging: " << e.what() << std::endl;
		}
		catch (...) {}
	}
	logger_session(console_color_t c, logger_t & l) NOEXCEPT;
	logger_session(endl_t const &, logger_t & l) NOEXCEPT;


	template <class T, class dummy = int>
	struct LogData
	{
		static inline void log(std::ostringstream & ss, T const & data) NOEXCEPT
		{
			try
			{
				ss << data;
			}
			catch (std::exception & e)
			{
				if (e.what())
					std::cerr << "Error while logging: " << e.what() << std::endl;
			}
			catch (...) {}
		}
	};

	template <class T, class dummy>
	struct LogData<T*, dummy>
	{
		static inline void log(std::ostringstream & ss, T *const data) NOEXCEPT
		{
			try
			{
				if (data)
				{
					ss << data;
				}
				else
					ss << typeid(T*).name() << "(NULL)";
			}
			catch (std::exception & e)
			{
				if (e.what())
					std::cerr << "Error while logging: " << e.what() << std::endl;
			}
			catch (...) {}
		}
	};

	template<class dummy>
	struct LogData<stream_array<char>, dummy>
	{
		static inline void log(std::ostringstream & ss, stream_array<char> const & data) NOEXCEPT
		{
			try
			{
				ss << std::string(data.data(), data.size());
			}
			catch (std::exception & e)
			{
				if (e.what())
					std::cerr << "Error while logging: " << e.what() << std::endl;
			}
			catch (...) {}
		}
	};

	template<unsigned len, class dummy>
	struct LogData<fixed_stream_array<char, len>, dummy>
	{
		static inline void log(std::ostringstream & ss, fixed_stream_array<char, len> const & data) NOEXCEPT
		{
			try
			{
				ss << to_string(data);
			}
			catch (std::exception & e)
			{
				if (e.what())
					std::cerr << "Error while logging: " << e.what() << std::endl;
			}
			catch (...) {}
		}
	};

	template<typename T, typename... ARGS>
	void log_impl_w2(const char * cmd, T const & value, ARGS && ... args)
	{
		LogData<T>::log(ss, value);
		print(cmd, std::forward<ARGS>(args)...);
	}


	template<typename T, typename... ARGS>
	void log_impl_w(bool b_precision_field, const char * cmd, T const & value, ARGS && ... args)
	{
		if (b_precision_field)
		{
			detail::log_set_precision<T>::set(ss, value);
			log_impl_w2(cmd, std::forward<ARGS>(args)...);
		}
		else
		{
			LogData<T>::log(ss, value);
			print(cmd, std::forward<ARGS>(args)...);
		}
	}

	inline void log_impl_w2(const char * cmd)
	{
		print(cmd);
	}


	inline void log_impl_w(bool b_precision_field, const char * cmd)
	{
		print(cmd);
	}

public:

	logger_session & print(const char *cmd)
	{
		while (*cmd)
		{
			if (*cmd == '%')
			{
				if (*(cmd + 1) == '%')
					++cmd;
			}
			ss << *cmd++;
		}
		return *this;
	}

	template<typename T, typename... ARGS>
	logger_session & print(const char *cmd, T const & value, ARGS && ... args)
	{
		while (*cmd)
		{
			if (*cmd == '%')
			{
				if (*(cmd + 1) == '%')
					++cmd;
				else
				{
					unsigned num = 0;
					bool b_precision = false;
					bool b_width_field = false;
					bool b_precision_field = false;

					for (++cmd; *cmd; ++cmd)
					{
						switch (*cmd)
						{
						case 'F':
							ss << std::uppercase;
						case 'i':
						case 'd':
						case 'u':
						case 'f':
							ss << std::dec;
							break;
						case 'o':
							ss << std::oct;
							break;
						case 'X':
							ss << std::uppercase;
						case 'x':
							ss << std::hex;
							break;
						case 'E':
						case 'G':
							ss << std::uppercase;
						case 'e':
						case 'g':
							ss << std::scientific;
							break;
						case 'A':
							ss << std::uppercase;
						case 'a':
//							ss << std::hexfloat;
							break;
						case 's':
						case 'c':
						case 'p':
							break;
						case 'n':
							break;
						case 'h':
						case 'z':
						case 't':
						case 'j':
						case 'L':
						case 'l':
							continue;
						case '-':
							ss << std::left;
							continue;
						case '#':
							ss << std::showbase << std::showpoint;
							continue;
						case '*':
							if (b_precision)
								b_precision_field = true;
							else
								b_width_field = true;
							continue;
						case '+':
							ss << std::showpos;
							continue;
						case ' ':
							continue;
						case '.':
							if (!b_precision && num)
								ss << std::setw(num);
							b_precision = true;
							num = 0;
							continue;
						case '0':
							if (num == 0 && !b_precision)
							{
								ss << std::setfill('0');
								continue;
							}
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							num = num * 10 + (*cmd - '0');
							continue;
						}

						++cmd;
						break;
					}

					if (!b_precision && num)
						ss << std::setw(num);
					if (b_precision && !b_precision_field)
						ss << std::setprecision(num);

					if (b_width_field)
					{
						detail::log_set_width<T>::set(ss, value);
						log_impl_w(b_precision_field, cmd, std::forward<ARGS>(args)...);
					}
					else if (b_precision_field)
					{
						detail::log_set_precision<T>::set(ss, value);
						log_impl_w2(cmd, std::forward<ARGS>(args)...);
					}
					else
					{
						LogData<T>::log(ss, value);
						print(cmd, std::forward<ARGS>(args)...);
					}

					return *this;
				}
			}
			ss << *cmd++;
		}

		return *this;
	}

	
	logger_session(logger_session && b)
        : l(b.l)
	{
        ss << b.ss.str();
		b.ss.str("");
	}
	

//	logger_session(logger_session && b) = default;

	~logger_session() NOEXCEPT;

	//logger_session& operator << (std::wstring const & data);

	template <class T>
	inline logger_session& operator << (T const & data) NOEXCEPT
	{
		LogData<T>::log(ss, data);
		return *this;
	}

	logger_session& operator << (console_color_t const & c) NOEXCEPT;
	logger_session& operator << (endl_t const & endl) NOEXCEPT;

	inline logger_session& operator << (std::ostream& (*pf)(std::ostream&)) NOEXCEPT
	{
		try
		{
			ss << pf;
		}
		catch (std::exception & e)
		{
			if (e.what())
				std::cerr << "Error while logging: " << e.what() << std::endl;
		}
		catch (...) {}
		return *this;
	}
	inline logger_session& operator << (std::ios& (*pf)(std::ios&)) NOEXCEPT
	{
		try
		{
			ss << pf;
		}
		catch (std::exception & e)
		{
			if (e.what())
				std::cerr << "Error while logging: " << e.what() << std::endl;
		}
		catch (...) {}
		return *this;
	}
	inline logger_session& operator << (std::ios_base& (*pf)(std::ios_base&)) NOEXCEPT
	{
		try
		{
			ss << pf;
		}
		catch (std::exception & e)
		{
			if (e.what())
				std::cerr << "Error while logging: " << e.what() << std::endl;
		}
		catch (...) {}
		return *this;
	}

};

class logger_t
{
	friend class logger_session;
	template <class T> friend struct logger_helper;
	std::ofstream f;
	std::string out_filename;
	//std::atomic_flag _lock;
	mutex_t mutex;
	volatile unsigned alive = 1;
//	std::thread::id last_thread;
	unsigned input_width;
	void logstr(const std::string & str);
	std::vector<std::string> inputs;
	std::vector<unsigned> cursor_pos;
	std::vector<unsigned> cursor_window;
	unsigned current_slot = 0;
	void(*listener)(std::string const &, unsigned) = nullptr;
public:
	void set_input_width(unsigned value)
	{
		input_width = value;
	}
	logger_t(char const * const filename, unsigned input_width = 79, unsigned input_slots = 20);

	void set_listener(void(*listener)(std::string const &, unsigned));

	bool handle_input(std::string & out);
	void cls();

	void shutdown();

	~logger_t();

	inline logger_session get_session()
	{
		return logger_session(*this);
	}

	template <class T>
	inline logger_session operator << (T const & data)
	{
		return logger_session(data, *this);
	}

	inline logger_session operator << (std::ostream& (*pf)(std::ostream&))
	{
		return logger_session(pf, *this);
	}
	inline logger_session operator << (std::ios& (*pf)(std::ios&))
	{
		return logger_session(pf, *this);
	}
	inline logger_session operator << (std::ios_base& (*pf)(std::ios_base&))
	{
		return logger_session(pf, *this);
	}

	void set_prefix(std::string && p);
	std::string reset_prefix();

	template <typename ... ARGS>
	inline logger_session print(const char *cmd, ARGS && ... args)
	{
		logger_session s(*this);
		s.print(cmd, std::forward<ARGS>(args)...);
		return std::move(s);
	}

};

extern endl_t const endl;

using std::boolalpha;
using std::dec;
//using std::endl;
using std::ends;
using std::fixed;
using std::flush;
using std::hex;
using std::internal;
using std::left;
using std::noboolalpha;
using std::noshowbase;
using std::noshowpoint;
using std::noshowpos;
using std::nounitbuf;
using std::nouppercase;
using std::oct;
using std::right;
using std::scientific;
using std::showbase;
using std::showpoint;
using std::showpos;
using std::unitbuf;
using std::uppercase;

using std::setw;
using std::setfill;

extern logger_t logger;


