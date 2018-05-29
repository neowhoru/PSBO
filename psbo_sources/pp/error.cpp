#include "stdafx.h"
#include "error.h"
#include "logging.h"

char const * const remove_path(const char * p) NOEXCEPT
{
	const char * ret = p;

	for (; *p != 0; ++p)
		if (*p == '\\' || *p == '/')
			ret = p + 1;

	return ret;
}

std::string remove_path(const std::string & path)
{
	unsigned ret = 0;

	for (unsigned p = 0; p < path.length(); ++p)
		if (path[p] == '\\' || path[p] == '/')
			ret = p + 1;

	return &path.c_str()[ret];
}

void _throw_simple_error(char const * const file, signed line, char const * const func, char const * const message)
{
	throw std::runtime_error((std::string)"Exception thrown at " + remove_path(file) + ':' + std::to_string((signed)line) + ' ' + func + " :\r\n\t" + message);
//	throw dwn_error(file, line, func) << message;
}

void _throw_simple_error(char const * const file, signed line, char const * const func, std::string const & message)
{
	throw std::runtime_error((std::string)"Exception thrown at " + remove_path(file) + ':' + std::to_string((signed)line) + ' ' + func + " :\r\n\t" + message);
}

void _throw_simple_error2(char const * const file, signed line, char const * const func, char const * const message1, const char * const message2)
{
	throw std::runtime_error((std::string)"Exception thrown at " + remove_path(file) + ':' + std::to_string((signed)line) + ' ' + func + " :\r\n\t" + message1 + message2);
}

void _print_boost_error(char const * const file, signed line, char const * const func, boost::system::error_code const & error)
{
	try
	{
		logger << LIGHT_RED << "At " << remove_path(file) << ':' << line << ' ' << func << " :\r\n\t error:"
			<< error.value() << ' ' << error << ' ' << error.message();
	}
	catch (...)
	{
	}
}

void log_catch_impl(const char * const file, unsigned line, const char * const funcname, const char * const message)
{
	try
	{
		logger << LIGHT_RED << "Exception caught at " << remove_path(file) << ':' << line << ' ' << funcname << " :" << "\r\n\t" << message;
	}
	catch (...) 
	{}
}
void runtime_assert_fail(const char * const file, unsigned line, const char * const funcname, const char * const assert_text)
{
	try
	{
		logger << LIGHT_RED << "Runtime assertion failed at " << remove_path(file) << ':' << line << ' ' << funcname << ": " << (assert_text ? assert_text : "NO_ASSERT_TEXT");
	}
	catch (...)
	{}
}

