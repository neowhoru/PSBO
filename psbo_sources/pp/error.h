#pragma once

#include <assert.h>
#include <stdexcept>
#include "string.h"

#if  (defined(_MSC_VER) && defined(_WIN32))
#define __FUNCTION_NAME__ __FUNCTION__
#else
#define __FUNCTION_NAME__ __func__
#endif


char const * const remove_path(const char * path) NOEXCEPT;
std::string remove_path(const std::string & path);

class logger_session;


void _throw_simple_error(char const * const file, signed line, char const * const func, char const * const message);
void _throw_simple_error(char const * const file, signed line, char const * const func, std::string const & message);
void _throw_simple_error2(char const * const file, signed line, char const * const func, char const * const message1, const char * const message2);
void _print_boost_error(char const * const file, signed line, char const * const func, boost::system::error_code const & error);
void log_catch_impl(const char * const file, unsigned line, const char * const funcname, const char * const message);
void runtime_assert_fail(const char * const file, unsigned line, const char * const funcname, const char * const assert_text = nullptr);


#define print_boost_error(m) _print_boost_error(__FILE__, __LINE__, __FUNCTION_NAME__, (m))
#define throw_simple_error(m) _throw_simple_error(__FILE__, __LINE__, __FUNCTION_NAME__, (m))
#define error_message() dwn_error(__FILE__, __LINE__, __FUNCTION_NAME__)
#define catch_message(e) log_catch_impl(__FILE__, __LINE__, __FUNCTION_NAME__, (e).what())
#define LOG_CATCH() catch(std::exception & e) { log_catch_impl(__FILE__, __LINE__, __FUNCTION_NAME__, e.what()); }


#define LOG_EX( expr ) try { expr ; } catch(std::exception & e) { log_catch_impl(__FILE__, __LINE__, __FUNCTION_NAME__, e.what()); }

#ifdef _DEBUG
#define assert_log(v) (assert((v)), false)
#else
#define assert_log(v) ((v) ? false : (runtime_assert_fail(__FILE__, __LINE__, __FUNCTION_NAME__), true))
#endif

#define assert2(r) BOOST_UNLIKELY(_assert2((bool)(r), __FILE__, __LINE__, __FUNCTION_NAME__, #r))

inline bool _assert2(bool r, const char * const file, unsigned line, const char * const funcname, const char * const assert_text)
{
	assert(r);
	if (BOOST_UNLIKELY(!r))
		runtime_assert_fail(file, line, funcname, assert_text);
	return r;
}


