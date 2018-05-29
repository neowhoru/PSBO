#include "stdafx.h"
#include "common.h"
#include "logging.h"
#include "platform_specific.h"



thread_local std::string * log_prefix = nullptr;

thread_local console_color_t default_color = console_color_t::GREY;
volatile unsigned current_color = console_color_t::GREY;
console_color_t edit_color = console_color_t::GREY;


endl_t const endl = endl_t();

logger_t logger("log.txt");  ///opens logfile



logger_session::logger_session(logger_t & l)
	: l(l)
{
}

logger_session::logger_session(console_color_t c, logger_t & l) NOEXCEPT
	: l(l)
{
	ss << (char)27 << (unsigned)c << (char)27;
}

logger_session::logger_session(endl_t const &, logger_t & l) NOEXCEPT
	: l(l)
{
	ss << '\r';
}

logger_session & logger_session::operator << (endl_t const & endl) NOEXCEPT
{
	ss << '\r';
	return *this;
}

logger_session & logger_session::operator <<(console_color_t const & c) NOEXCEPT
{
	ss << (char)27 << (unsigned)c << (char)27;
	return *this;
}

logger_session::~logger_session() NOEXCEPT
{
	try
	{
		std::string const & str = ss.str();
		if (str.length())
			l.logstr(str);
	}
	catch (...) {}
}

void logger_t::cls()
{
//	while (_lock.test_and_set(std::memory_order_acquire)) { ; }
	//std::lock_guard<std::mutex> lock(mutex);
	impl_lock_guard(lock, mutex);
	clear_screen();

	if (inputs[current_slot].size())
	{
		if (current_color != edit_color)
		{
			set_console_color(edit_color);
			current_color = edit_color;
		}
#ifndef AS_DAEMON
		std::cout << '\r';
#endif
		if (inputs[current_slot].size() < input_width)
		{
			cursor_window[current_slot] = 0;
#ifndef AS_DAEMON
			std::cout << inputs[current_slot] << ' ' << (char)8;
			for (unsigned pos = inputs[current_slot].length(); pos > cursor_pos[current_slot]; --pos)
				std::cout << (char)8;
#endif
		}
		else
		{
			if (cursor_pos[current_slot] > cursor_window[current_slot] + input_width)
				cursor_window[current_slot] = cursor_pos[current_slot] - input_width;
			if (cursor_pos[current_slot] < cursor_window[current_slot])
				cursor_window[current_slot] = cursor_pos[current_slot];
			if ((int)cursor_window[current_slot] < 0)
				cursor_window[current_slot] = 0;
			if (cursor_window[current_slot] > inputs[current_slot].size() - input_width)
				cursor_window[current_slot] = inputs[current_slot].size() - input_width;
#ifndef AS_DAEMON
			std::cout << inputs[current_slot].substr(cursor_window[current_slot], input_width);
			for (unsigned pos = cursor_window[current_slot] + input_width; pos > cursor_pos[current_slot]; --pos)
				std::cout << (char)8;
#endif
		}
	}

	if (listener)
		listener(std::string(), ~0);

//	_lock.clear(std::memory_order_release);
}

void logger_t::logstr(const std::string & str)
{
	if (alive == 0)
		return;

	if (!f.is_open())
	{
		f.open(out_filename, std::ios_base::out | std::ios_base::app);//1
		if ((long)f.tellp() == 0)
			f << (unsigned char)0xEF << (unsigned char)0xBB << (unsigned char)0xBF;
	}

	char buffer[1024];
	unsigned bp = 0;

//	bool acquired = false;
	bool on_newline = true;
	std::string const entry_date = '[' + datetime2str_2() + "] ";

	//std::lock_guard<std::mutex> lock(mutex);
	impl_lock_guard(lock, mutex);

//	if (!acquired)
//	{
//		lock_atomic_flag(_lock);
//		acquired = true;
//	}

	if (inputs[current_slot].size() > 0)
	{
#ifndef AS_DAEMON
		std::cout << "\r" << std::string(std::min<unsigned>(inputs[current_slot].size(), input_width), ' ') << "\r";
#endif
	}

	//unsigned current_color = default_color;
	if (current_color != default_color)
	{
		if (str.size() > 0 && str[0] == 27)
			;
		else
		{
			current_color = default_color;
			set_console_color(current_color);
		}
	}

	try
	{
		unsigned size = str.size();
		for (unsigned i = 0; i < size; ++i)
		{
			char ch = str[i];

			switch (ch)
			{
			case 10:
				continue;
			case 13:
				if (bp >= sizeof(buffer) - 1)
				{
//					if (!acquired)
//					{
//						while (_lock.test_and_set(std::memory_order_acquire)) { ; }
//						acquired = true;
//					}

#ifndef AS_DAEMON
					std::cout.write(buffer, bp);
#endif

					if (f.is_open())
						f.write(buffer, bp);
					if (listener)
						listener(std::string(buffer, bp), current_color);
					bp = 0;
				}
#ifdef WIN32
				buffer[bp] = '\r';
				++bp;
#endif
				buffer[bp] = '\n';
				++bp;
				on_newline = true;
				continue;
			case 27:
			{
				if (bp > 0)
				{
//					if (!acquired)
//					{
//						while (_lock.test_and_set(std::memory_order_acquire)) { ; }
//						acquired = true;
//					}

#ifndef AS_DAEMON
					std::cout.write(buffer, bp);
#endif
					if (f.is_open())
						f.write(buffer, bp);
					if (listener)
						listener(std::string(buffer, bp), current_color);
					bp = 0;
				}
				unsigned color = 0;
				for (++i; i < size; ++i)
				{
					if (str[i] == 27)
						break;
					color = color * 10 + (str[i] - '0');
				}
				if (color != current_color)
				{
					set_console_color(color);
					current_color = color;
				}
			}
			break;

			default:
				if (on_newline)
				{
					if (std::string * p = log_prefix)
					{
						if (p->size() < sizeof(buffer) && p->size() > 0)
						{
							if (sizeof(buffer) - bp < p->size())
							{
//								if (!acquired)
//								{
//									while (_lock.test_and_set(std::memory_order_acquire)) { ; }
//									acquired = true;
//								}

#ifndef AS_DAEMON
								std::cout.write(buffer, bp);
#endif
								if (f.is_open())
									f.write(buffer, bp);
								if (listener)
									listener(std::string(buffer, bp), current_color);
								bp = 0;
							}
							for (unsigned j = 0; j < p->size(); ++j, ++bp)
								buffer[bp] = (*p)[j];
						}
					}
					{
						std::string const & p = entry_date;
						if (p.size() < sizeof(buffer) && p.size() > 0)
						{
							if (sizeof(buffer) - bp < p.size())
							{
#ifndef AS_DAEMON
								std::cout.write(buffer, bp);
#endif
								if (f.is_open())
									f.write(buffer, bp);
								if (listener)
									listener(std::string(buffer, bp), current_color);
								bp = 0;
							}
							for (unsigned j = 0; j < p.size(); ++j, ++bp)
								buffer[bp] = (p)[j];
						}
					}

					on_newline = false;
				}

				assert(bp <= sizeof(buffer));

				if (bp == sizeof(buffer))
				{
//					if (!acquired)
//					{
//						while (_lock.test_and_set(std::memory_order_acquire)) { ; }
//						acquired = true;
//					}

#ifndef AS_DAEMON
					std::cout.write(buffer, bp);
#endif
					if (f.is_open())
						f.write(buffer, bp);
					if (listener)
						listener(std::string(buffer, bp), current_color);
					bp = 0;
				}
				buffer[bp] = ch;
				++bp;
			}
		}
	}
	catch (std::exception & e)
	{
		if (e.what())
			std::cerr << "Error while logging: " << e.what() << std::endl;
	}
	catch (...) {}

//	if (!acquired)
//	{
//		while (_lock.test_and_set(std::memory_order_acquire)) { ; }
//		acquired = true;
//	}

	if (bp > 0)
	{
#ifndef AS_DAEMON
		std::cout.write(buffer, bp);
#endif
		if (f.is_open())
			f.write(buffer, bp);
		if (listener)
			listener(std::string(buffer, bp), current_color);
		bp = 0;
	}

	if (!on_newline)
	{

#ifndef AS_DAEMON
#ifdef WIN32
		std::cout << "\r\n";
#else
		std::cout << '\n';
#endif
#endif
		if (f.is_open())
			f << std::endl;
		if (listener)
			listener(std::string(1, (char)13), current_color);
	}
	else if (f.is_open())
		f.flush();

	if(inputs[current_slot].size() > 0)
	{
		if (current_color != edit_color)
		{
			set_console_color(edit_color);
			current_color = edit_color;
		}

		if (inputs[current_slot].size() < input_width)
		{
			cursor_window[current_slot] = 0;
#ifndef AS_DAEMON
			std::cout << inputs[current_slot] << ' ' << (char)8;
			for (unsigned pos = inputs[current_slot].length(); pos > cursor_pos[current_slot]; --pos)
				std::cout << (char)8;
#endif
		}
		else
		{
			if (cursor_pos[current_slot] > cursor_window[current_slot] + input_width)
				cursor_window[current_slot] = cursor_pos[current_slot] - input_width;
			if (cursor_pos[current_slot] < cursor_window[current_slot])
				cursor_window[current_slot] = cursor_pos[current_slot];
			if ((int)cursor_window[current_slot] < 0)
				cursor_window[current_slot] = 0;
			if (cursor_window[current_slot] > inputs[current_slot].size() - input_width)
				cursor_window[current_slot] = inputs[current_slot].size() - input_width;
#ifndef AS_DAEMON
			std::cout << inputs[current_slot].substr(cursor_window[current_slot], input_width);
			for (unsigned pos = cursor_window[current_slot] + input_width; pos > cursor_pos[current_slot]; --pos)
				std::cout << (char)8;
#endif
		}
	}

//	if (acquired)
//		_lock.clear(std::memory_order_release);

}

void logger_t::set_listener(void(*listener)(std::string const &, unsigned))
{
//	while (_lock.test_and_set(std::memory_order_acquire)) { ; }
	//std::lock_guard<std::mutex> lock(mutex);
	impl_lock_guard(lock, mutex);
	this->listener = listener;
//	_lock.clear(std::memory_order_release);
}

bool logger_t::handle_input(std::string & out)
{
	bool r = false;

#ifdef AS_DAEMON
	return r;
#endif

	bool changed = true;

	if (pp_kbhit())
	{
		std::vector<int> characters;
		do
		{
			int ch = pp_getch();
			characters.push_back(ch);
			if (ch == 0 || (ch & 0xff) == 0xe0 || ch == 27)
			{
				if (ch == 27)
					characters.push_back(pp_getch());
				characters.push_back(pp_getch());
			}

#ifdef _WIN32
			if (ch == 13)
				break;
#else
			if (ch == 10)
				break;
#endif

		} while (pp_kbhit());

		//std::lock_guard<std::mutex> lock(mutex);
		impl_lock_guard(lock, mutex);
//		while (_lock.test_and_set(std::memory_order_acquire)) { ; }

		if (current_color != default_color)
		{
			current_color = default_color;
			set_console_color(current_color);
		}

		try
		{
			for (unsigned _i = 0; _i < characters.size(); ++_i)
			{
				int ch = characters[_i];
				if (ch == 0 || (ch & 0xff) == 0xe0 || ch == 27)
				{
					if (ch == 27)
						++_i;

					++_i;
					ch = _i < characters.size() ? characters[_i] : 0;

					switch (ch)
					{
					case 71:	//home
						if (cursor_pos[current_slot] != 0)
						{
							cursor_pos[current_slot] = 0;
							changed = true;
						}
						break;
					case 79:	//end
						if (cursor_pos[current_slot] != inputs[current_slot].size())
						{
							cursor_pos[current_slot] = inputs[current_slot].size();
							changed = true;
						}
						break;
					case 75:	//left
						if (cursor_pos[current_slot] > 0)
						{
							--cursor_pos[current_slot];
							changed = true;
						}
						break;
					case 77:	//right
						if (cursor_pos[current_slot] < inputs[current_slot].size())
						{
							++cursor_pos[current_slot];
							changed = true;
						}
						break;
					case 72:	//up
						if (inputs.size() > 1 && current_slot > 0)
						{
							unsigned next_slot = (current_slot - 1) % inputs.size();
#ifndef AS_DAEMON
							if (inputs[current_slot].size() > 0)
								std::cout << '\r' << std::string(std::min<unsigned>(input_width, inputs[current_slot].size()), ' ') << '\r';
#endif
							current_slot = next_slot;
							//cursor_pos = inputs[current_slot].size();
							changed = true;
						}
						break;
					case 80:	//down
						if (inputs.size() > 1 && current_slot != inputs.size() - 1)
						{
							unsigned next_slot = (current_slot + 1) % inputs.size();
#ifndef AS_DAEMON
							if (inputs[current_slot].size() > 0)
								std::cout << '\r' << std::string(std::min<unsigned>(input_width, inputs[current_slot].size()), ' ') << '\r';
#endif
							current_slot = next_slot;
							//cursor_pos = inputs[current_slot].size();
							changed = true;
						}
						break;
					case 83:	//delete
						if (cursor_pos[current_slot] < inputs[current_slot].size())
						{
							inputs[current_slot] = inputs[current_slot].substr(0, cursor_pos[current_slot]) + inputs[current_slot].substr(cursor_pos[current_slot] + 1);
							changed = true;
						}
						break;
					}
				}
#ifdef _WIN32
				else if (ch == 10) { ; }
				else if (ch == 13)
#else
				else if (ch == 13) { ; }
				else if (ch == 10)
#endif
				{
					if (inputs[current_slot].size())
					{
#ifndef AS_DAEMON
						std::cout << '\r' << std::string(std::min<unsigned>(input_width, inputs[current_slot].size()), ' ') << '\r';
#endif
						//if (inputs.size() > 0)
						{
							if (current_slot == inputs.size() - 1)
							{
								for (unsigned i = 1; i < inputs.size(); ++i)
								{
									inputs[i - 1] = std::move(inputs[i]);
									cursor_pos[i - 1] = cursor_pos[i];
									cursor_window[i - 1] = cursor_window[i];
								}
								//inputs[current_slot] = current_input;
								if (inputs.size() > 1)
									out = inputs[current_slot - 1];
								else
									out = inputs[current_slot];

								inputs[current_slot].clear();
							}
							else
							{
								//inputs[current_slot] = current_input;
								out = inputs[current_slot];
								current_slot = (current_slot + 1) % inputs.size();
								inputs[current_slot].clear();
							}
						}
						r = true;
						changed = false;
						cursor_pos[current_slot] = inputs[current_slot].size();
					}
				}
				else if (ch == 8 && inputs[current_slot].size() && cursor_pos[current_slot] > 0)
				{
					inputs[current_slot] = inputs[current_slot].substr(0, cursor_pos[current_slot] - 1) + inputs[current_slot].substr(cursor_pos[current_slot]);
					--cursor_pos[current_slot];
					//inputs[current_slot].resize(inputs[current_slot].size() - 1);
					changed = true;
				}
				else if (ch >= 32 || ch == 9)
				{
					inputs[current_slot].insert(cursor_pos[current_slot], 1, (char)ch);
					++cursor_pos[current_slot];

					//inputs[current_slot].insert(inputs[current_slot].size(), 1, (char)ch);
					//current_input += (char)ch;
					changed = true;
				}
			}//while (!r && pp_kbhit());
		}
		catch (std::exception & e)
		{
			if (e.what())
				std::cerr << "Error while logging: " << e.what() << std::endl;
		}
		catch (...) {}

		if (changed)
		{
			if (current_color != edit_color)
			{
				set_console_color(edit_color);
				current_color = edit_color;
			}
#ifndef AS_DAEMON
			std::cout << '\r';
#endif
			if (inputs[current_slot].size() < input_width)
			{
				cursor_window[current_slot] = 0;
#ifndef AS_DAEMON
				std::cout << inputs[current_slot] << ' ' << (char)8;
				for (unsigned pos = inputs[current_slot].length(); pos > cursor_pos[current_slot]; --pos)
					std::cout << (char)8;
#endif
			}
			else
			{
				if (cursor_pos[current_slot] > cursor_window[current_slot] + input_width)
					cursor_window[current_slot] = cursor_pos[current_slot] - input_width;
				if (cursor_pos[current_slot] < cursor_window[current_slot])
					cursor_window[current_slot] = cursor_pos[current_slot];
				if ((int)cursor_window[current_slot] < 0)
					cursor_window[current_slot] = 0;
				if (cursor_window[current_slot] > inputs[current_slot].size() - input_width)
					cursor_window[current_slot] = inputs[current_slot].size() - input_width;
#ifndef AS_DAEMON
				std::cout << inputs[current_slot].substr(cursor_window[current_slot], input_width);
				for (unsigned pos = cursor_window[current_slot] + input_width; pos > cursor_pos[current_slot]; --pos)
					std::cout << (char)8;
#endif
			}
		}

//		_lock.clear(std::memory_order_release);
	}

	return r;
}

logger_t::logger_t(char const * const filename, unsigned input_width, unsigned input_slots)
	//: f(filename, std::ios_base::out | std::ios_base::app)
	: out_filename(filename)
	, input_width(input_width)
{
//	if (!_lock.test_and_set(std::memory_order_acquire))
//		_lock.clear(std::memory_order_release);

	inputs.resize(std::max<unsigned>(1, input_slots));
	cursor_pos.resize(inputs.size());
	cursor_window.resize(inputs.size());

}

logger_t::~logger_t()
{
	std::atomic_thread_fence(std::memory_order_seq_cst);
	alive = 0;
	std::atomic_thread_fence(std::memory_order_seq_cst);
	try
	{
		if (f.is_open())
		{
			f.flush();
			f.close();
		}
	}
	catch (...) {}
}
/*
///flushes the log-file
void logger_t::flush()
{
try
{
if (f.is_open())
f.flush();
}
catch (...) {}
}
*/
///shutting the log down (in case of a process-exception (see platform_specific.cpp))
void logger_t::shutdown()
{
	try
	{
		if (f.is_open())
		{
			f.flush();
			f.close();
		}
	}
	catch (...) {}
}

void logger_t::set_prefix(std::string && p)
{
	try
	{
		safe_delete(log_prefix);
		log_prefix = cnew std::string(std::move(p));
	}
	catch (...)
	{
		log_prefix = nullptr;
	}
}

std::string logger_t::reset_prefix()
{
	std::string old_prefix;
	try
	{
		if (log_prefix)
			old_prefix = *log_prefix;
		safe_delete(log_prefix);
	}
	catch (...)
	{
		log_prefix = nullptr;
	}
	return old_prefix;
}

