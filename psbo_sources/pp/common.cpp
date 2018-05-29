#include "stdafx.h"
#include "common.h"

const std::string sha_license = "\
/*\r\n\
* Updated to C++, zedwood.com 2012\r\n\
* Based on Olivier Gay's version\r\n\
* See Modified BSD License below:\r\n\
*\r\n\
* FIPS 180-2 SHA-224/256/384/512 implementation\r\n\
* Issue date:  04/30/2005\r\n\
* http://www.ouah.org/ogay/sha2/\r\n\
*\r\n\
* Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>\r\n\
* All rights reserved.\r\n\
*\r\n\
* Redistribution and use in source and binary forms, with or without\r\n\
* modification, are permitted provided that the following conditions\r\n\
* are met:\r\n\
* 1. Redistributions of source code must retain the above copyright\r\n\
*    notice, this list of conditions and the following disclaimer.\r\n\
* 2. Redistributions in binary form must reproduce the above copyright\r\n\
*    notice, this list of conditions and the following disclaimer in the\r\n\
*    documentation and/or other materials provided with the distribution.\r\n\
* 3. Neither the name of the project nor the names of its contributors\r\n\
*    may be used to endorse or promote products derived from this software\r\n\
*    without specific prior written permission.\r\n\
*\r\n\
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND\r\n\
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\r\n\
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\r\n\
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE\r\n\
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\r\n\
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\r\n\
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\r\n\
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\r\n\
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\r\n\
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\r\n\
* SUCH DAMAGE.\r\n\
*/\r\n\
";

mutex_t rnd_base_t::rd_mutex;
std::random_device rnd_base_t::rd;


std::string trim(const std::string & str)
{
	unsigned i = 0, j = str.length() - 1;
	for (; i < str.length(); ++i)
	{
		if ((unsigned int)(unsigned char)str[i] >(unsigned int)32)
			break;
	}
	for (; j != (unsigned)~0; --j)
	{
		if ((unsigned int)(unsigned char)str[j] > (unsigned int)32)
			break;
	}

	return str.substr(i, j + 1 - i);
}

std::vector<std::string> split(std::string const & instr, unsigned split_char)
{
	std::vector<std::string> r;
	unsigned cp;
	std::string current;
	for (unsigned i = 0; i < instr.length();)
	{
		cp = instr[i];
		++i;
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


std::vector<std::string> split2(std::string const & instr, unsigned split_char)
{
	std::vector<std::string> r;
	unsigned cp;
	std::string current;
	unsigned in_string = 0;
	for (unsigned i = 0; i < instr.length();)
	{
		cp = instr[i];
		++i;
		if (cp != split_char || in_string)
		{
			current += (char)cp;
			if (cp == '"' || cp == '\'')
			{
				if (in_string == 0)
					in_string = cp;
				else if (in_string == cp)
					in_string = 0;
			}
		}
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


std::string to_lower(const std::string & str)
{
	std::string r = str;

	for (char & ch : r)
	{
		if (ch >= 'A' && ch <= 'Z')
			ch += 'a' - 'A';
	}

	return r;
}

std::vector<std::vector<std::string> > from_csv(std::string const & instr, unsigned separator)
{
	std::vector<std::vector<std::string> > r;
	std::vector<std::string> lines = split(instr, 10);
	r.reserve(lines.size());
	for (std::string const & line : lines)
	{
		r.push_back(split(line, separator));
		for (std::string & word : r.back())
			word = trim(word);
	}
	return r;
}

std::string open_file(std::string const & filename)
{
	std::string str_r;
	FILE * f1 = fopen(filename.c_str(), "r+b");
	if (f1)
	{
		fseek(f1, 0, SEEK_END);
		unsigned size = ftell(f1);
		fseek(f1, 0, SEEK_SET);

		std::vector<char> v;
		v.resize(size);

		unsigned read_count = 0;
		int r;
		for (unsigned retry = 0; read_count < size && retry < 3; ++retry)
		{
			if (retry > 0)
				sleep_for(1000);

			do
			{
				r = fread(&v[read_count], 1, size - read_count, f1);
				if (r > 0)
					read_count += r;
			} while (r > 0 && read_count < size);
		}
		fclose(f1);

		if (read_count < size)
			throw std::runtime_error("Couldn't read file " + filename);

		str_r.assign(v.begin(), v.end());
	}
	else
		throw std::runtime_error("Failed to open file " + filename);

	return str_r;
}

namespace
{
	mutex_t localtime_mutex;
}

struct tm get_localtime(time_t t)
{
	struct tm r;
	memset(&r, 0, sizeof(r));
	
	{
		impl_lock_guard(lock, localtime_mutex);
		if (struct tm * now = localtime(&t))
			r = *now;
	}

	return r;
}


std::string datetime2str(time_t t, char separator1, char separator2)
{
	char p[4096];
	struct tm now = get_localtime(t);
	sprintf(&p[0], "%d%c%02d%c%02d %02d%c%02d%c%02d", now.tm_year + 1900, separator1, (now.tm_mon + 1), separator1, now.tm_mday, now.tm_hour, separator2, now.tm_min, separator2, now.tm_sec);
	p[sizeof(p) - 1] = 0;

	return p;
}


std::string datetime2str_2(time_t t)
{
	char p[4096];
	struct tm now = get_localtime(t);
	sprintf(&p[0], "%02d%02d%02d %02d%02d%02d", (now.tm_year + 1900) % 100, (now.tm_mon + 1), now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
	p[sizeof(p) - 1] = 0;

	return p;
}

