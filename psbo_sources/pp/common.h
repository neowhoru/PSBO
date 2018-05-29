#pragma once

#define ERROR_MESSAGE(e) remove_path(__FILE__) << ":" << __LINE__ << ": " << e.what()
#define NON_COPYABLE(TYPE) TYPE ( TYPE const & ) = delete; TYPE & operator = ( TYPE const & ) = delete;

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#include <type_traits>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <array>
#include <memory>
#include <functional>
#include <string.h>
#include <vector>
#include <mutex>
#include <ctime>
#include <random>
#include "error.h"
#include "platform_specific.h"
#include "logging.h"
#include "binarystream.h"

extern const std::string sha_license;

#if  (defined(_MSC_VER) && defined(_WIN32))
#define __FUNCTION_NAME__ __FUNCTION__
#else
#define __FUNCTION_NAME__ __func__
#endif

std::string datetime2str(time_t t = time(0), char year_separator = '-', char time_separator = ':');
std::string datetime2str_2(time_t t = time(0));

template <class T>
inline void safe_delete(T * & ptr)
{
	T * _ptr = ptr;
	ptr = nullptr;
	delete _ptr;
}

template <class T>
inline void safe_delete(T * volatile & ptr)
{
	T * _ptr = ptr;
	ptr = nullptr;
	delete _ptr;
}

template <class T>
inline T next_p2(T a) NOEXCEPT
{
	T rval = 1;
	while (rval < a && rval != 0)
		rval <<= 1;
	return rval;
}

template <class T>
inline T l2(T a) NOEXCEPT
{
	T r = 0;
	while (a >>= 1)
		++r;
	return r;
}

constexpr inline bool is_p2(unsigned x)
{
	return ((x != 0) && !(x & (x - 1)));
}

constexpr inline bool is_p2(unsigned long long x)
{
	return ((x != 0) && !(x & (x - 1)));
}

template <unsigned index, typename ... ARGS>
struct get_vararg;

template <typename ARG0, typename ... ARGS>
struct get_vararg<0, ARG0, ARGS...>
{
	typedef ARG0 type;
};

template <unsigned index, typename ARG0, typename ... ARGS>
struct get_vararg<index, ARG0, ARGS...>
{
	typedef typename get_vararg<index - 1, ARGS...>::type type;
};

namespace detail
{
	template<class T, class U, bool is_conv = std::is_convertible<T, U>::value>
	struct convert_helper
	{
		typedef U const & type;
		inline static U const & convert(T const & value)
		{
			return value;
		}
	};

	template <class U>
	struct convert_helper<std::string, U, false>
	{
		typedef U type;
		static U convert(std::string const & value)
		{
			std::istringstream iss(value);
			U r;
			iss >> r;
			return r;
		}
	};

	template <class T>
	struct convert_helper<T, std::string, false>
	{
		typedef std::string type;
		static std::string convert(T const & value)
		{
			std::ostringstream oss;
			oss << value;
			return oss.str();
		}
	};

	template <class T, class U>
	struct convert_helper<T, U, false>
	{
		typedef U type;
		static U convert(T const & value)
		{
			std::ostringstream oss;
			oss << value;
			std::istringstream iss(oss.str());
			U r;
			iss >> r;
			return r;
		}
	};
}

template <class U, class T>
inline typename detail::convert_helper<T, U>::type convert(T const & value)
{
	return detail::convert_helper<T, U>::convert(value);
}

template <class U, class T>
inline void convert(T const & value, U & out)
{
	out = detail::convert_helper<T, U>::convert(value);
}

template<unsigned len>
std::array<char, len> str2array(std::string const & str)
{
	std::array<char, len> r;
	unsigned i = 0;
	for (; i < (len - 1) && i < str.size(); ++i)
		r[i] = str[i];
	for (; i < len; ++i)
		r[i] = 0;

	return r;
}

template<unsigned len>
std::array<char, len> array_n(char def)
{
	std::array<char, len> r;
	unsigned i = 0;
	for (; i < len; ++i)
		r[i] = def;
	return r;
}


std::vector<std::string> split(std::string const & instr, unsigned split_char);
std::vector<std::string> split2(std::string const & instr, unsigned split_char);
std::string trim(const std::string & str);
std::string to_lower(const std::string & str);
std::vector<std::vector<std::string> > from_csv(std::string const & instr, unsigned separator = ',');
std::string open_file(std::string const & filename);


template <unsigned len>
std::vector<std::string> split(fixed_stream_array<char, len> const & instr, unsigned split_char)
{
	std::vector<std::string> r;
	unsigned cp;
	std::string current;
	for (unsigned i = 0; i < instr.size();)
	{
		cp = instr[i];
		++i;
		if (cp == 0)
			break;
		if (cp != split_char)
			current += (char)cp;
		else
		{
			r.push_back(std::move(current));
			current.clear();
		}
	}

	if (current.length())
		r.push_back(std::move(current));

	return r;
}


template <class T>
bool erase(std::vector<T> & vec, T const & value)
{
	for (unsigned i = 0; i < vec.size(); ++i)
	{
		if (vec[i] == value)
		{
			vec[i] = std::move(vec.back());
			vec.pop_back();
			return true;
		}
	}
	return false;
}

inline void sleep_for(unsigned ms)
{
#ifdef STD_HAS_THREADS
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#else
	boost::this_thread::sleep_for(boost::chrono::milliseconds(ms));
#endif
}

namespace detail
{
	template <unsigned s>
	struct unsigned_by_size;

	template<>
	struct unsigned_by_size<4>
	{
		typedef unsigned type;
	};

	template<>
	struct unsigned_by_size<8>
	{
		typedef unsigned long long type;
	};
}

template <class T>
inline void invalidate(T && value)
{
	char temp[sizeof(T) + alignof(T)];
	new(temp + alignof(T) - ((typename detail::unsigned_by_size<sizeof(char*)>::type)temp % alignof(T))) T (std::move(value));
}

struct tm get_localtime(time_t t);


class rnd_base_t
{
	static mutex_t rd_mutex;
	static std::random_device rd;
protected:
	std::random_device::result_type seed;
	rnd_base_t()
	{
		try
		{
			impl_lock_guard(lock, rd_mutex);
			seed = rd();
		}
		catch (...)
		{
			seed = time(0);
		}
	}
};

class rng
	: private rnd_base_t
{
	std::mt19937 re;

	rng()
		: re(rnd_base_t::seed)
	{
	}

public:


	static int gen_int(int min, int max)
	{
		static thread_local rng r;

		std::uniform_int_distribution<int> distribution(min, max);
		return std::min<int>(max, std::max<int>(min, distribution(r.re)));
	}
	static unsigned gen_uint(unsigned min, unsigned max)
	{
		static thread_local rng r;

		std::uniform_int_distribution<unsigned> distribution(min, max);
		return std::min<unsigned>(max, std::max<unsigned>(min, distribution(r.re)));
	}
};


class queue_limiter_t final
{
	std::atomic<unsigned> count;
	std::atomic<unsigned> max_count;
public:
	queue_limiter_t(unsigned max)
		: count(0)
		, max_count(max)
	{}

	unsigned get() const
	{
		return count;
	}

	unsigned get_max() const
	{
		return max_count;
	}
	void set_max(unsigned value)
	{
		max_count = std::max<unsigned>(1, value);
	}

	void dec()
	{
		--count;
	}

	void inc()
	{
		++count;
	}

	template <class T>
	bool post(T handler)
	{
		for (unsigned exp = count;;)
		{
			if (exp >= max_count)
			{
				sleep_for(10);
				exp = count;
				continue;
			}
			if (count.compare_exchange_weak(exp, exp + 1))
				return handler();
/*
#ifndef _NO_X86
			_mm_pause();
#endif
*/
		}

		return false;
	}

	template <class T, class R>
	bool post(T handler, R is_running)
	{
		for (unsigned exp = count; is_running();)
		{
			if (exp >= max_count)
			{
				sleep_for(10);
				exp = count;
				continue;
			}
			if (count.compare_exchange_weak(exp, exp + 1))
				return handler();

#ifndef _NO_X86
//			_mm_pause();
#endif

		}

		return false;
	}

};


class queue_limiter_dec_guard_t final
{
	queue_limiter_dec_guard_t(queue_limiter_dec_guard_t const &) = delete;
	queue_limiter_dec_guard_t& operator = (queue_limiter_dec_guard_t const & b) = delete;
	queue_limiter_t * l;
public:
	queue_limiter_dec_guard_t()
		: l(nullptr)
	{}
	queue_limiter_dec_guard_t(queue_limiter_t & _l)
		: l(&_l)
	{
	}

	queue_limiter_dec_guard_t(queue_limiter_dec_guard_t && b)
		: l(b.l)
	{
		b.l = nullptr;
	}

	queue_limiter_dec_guard_t& operator= (queue_limiter_dec_guard_t && b)
	{
		if (&b != this)
		{
			dec();
			l = b.l;
			b.l = nullptr;
		}
		return *this;
	}

	~queue_limiter_dec_guard_t()
	{
		dec();
	}

	void dec()
	{
		if (l)
		{
			queue_limiter_t * l1 = l;
			l = nullptr;
			l1->dec();
		}
	}

	void reset()
	{
		l = nullptr;
	}
};
