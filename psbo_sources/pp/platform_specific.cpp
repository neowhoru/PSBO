#include "stdafx.h"
#include "platform_specific.h"

#include "logging.h"
#include <thread>
#include <chrono>
#include <boost/assert.hpp>
#include <atomic>
#include "common.h"
#include <stdio.h>
#ifndef __GNUC__
#include <codecvt>
#else
#include <boost/locale/encoding_utf.hpp>
#endif

#ifdef AS_DAEMON
#include <syslog.h>
#endif

//#ifndef _MSC_VER
//#include <ctime>
//#endif


#ifdef _WIN32

#include <windows.h>
#include <wincon.h>
#include <conio.h>
//#include <dbghelp.h>

#pragma warning(disable:4996)	//CRT Secure warning

void on_crash();

namespace
{
	const unsigned char hex_nums[17] = "0123456789ABCDEF";
}
/*
void CreateMiniDump(EXCEPTION_POINTERS* pep)
{
	HANDLE hFile = CreateFile(L"MiniDump.dmp", GENERIC_READ | GENERIC_WRITE,
		0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei;

		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = pep;
		mdei.ClientPointers = FALSE;

		MINIDUMP_TYPE mdt = MiniDumpNormal;

		BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
			hFile, mdt, (pep != 0) ? &mdei : 0, 0, 0);

		CloseHandle(hFile);

	}
	else
	{
		logger << "Failed to create minidump file";
	}

}

*/
LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS* pep)
{
	_EXCEPTION_RECORD * per = pep->ExceptionRecord;
	bool cont = per->ExceptionCode != EXCEPTION_BREAKPOINT && per->ExceptionCode != EXCEPTION_SINGLE_STEP;

	//	if(per->ExceptionFlags or !cont)
	{
		logger << "Runtime exception ";
		for (; per != 0; per = per->ExceptionRecord)
		{
			switch (per->ExceptionCode)
			{
#define __tmp_(n) case n : logger << #n; break;
				__tmp_(EXCEPTION_ACCESS_VIOLATION);
				__tmp_(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
				__tmp_(EXCEPTION_BREAKPOINT);
				__tmp_(EXCEPTION_DATATYPE_MISALIGNMENT);
				__tmp_(EXCEPTION_FLT_DENORMAL_OPERAND);
				__tmp_(EXCEPTION_FLT_DIVIDE_BY_ZERO);
				__tmp_(EXCEPTION_FLT_INEXACT_RESULT);
				__tmp_(EXCEPTION_FLT_INVALID_OPERATION);
				__tmp_(EXCEPTION_FLT_OVERFLOW);
				__tmp_(EXCEPTION_FLT_STACK_CHECK);
				__tmp_(EXCEPTION_FLT_UNDERFLOW);
				__tmp_(EXCEPTION_ILLEGAL_INSTRUCTION);
				__tmp_(EXCEPTION_IN_PAGE_ERROR);
				__tmp_(EXCEPTION_INT_DIVIDE_BY_ZERO);
				__tmp_(EXCEPTION_INT_OVERFLOW);
				__tmp_(EXCEPTION_INVALID_DISPOSITION);
				__tmp_(EXCEPTION_NONCONTINUABLE_EXCEPTION);
				__tmp_(EXCEPTION_PRIV_INSTRUCTION);
				__tmp_(EXCEPTION_SINGLE_STEP);
				__tmp_(EXCEPTION_STACK_OVERFLOW);
#undef __tmp_
			default:
				logger << "Unknown exception code: " << hex << per->ExceptionCode;
			}
			logger << endl;

			logger << "Address: " << hex << setw(8) << setfill('0') << per->ExceptionAddress << endl;
			logger << "Flags: 0x" << hex << setw(8) << setfill('0') << per->ExceptionFlags << endl;

			switch (per->ExceptionCode)
			{
			case EXCEPTION_ACCESS_VIOLATION:
				if (per->NumberParameters > 0)
				{
					logger << "Cause: ";
					switch (per->ExceptionInformation[0])
					{
					case 0:
						logger << "read";
						break;
					case 1:
						logger << "write";
						break;
					case 8:
						logger << "dep";
						break;
					default:
						logger << "flag(" << per->ExceptionInformation[0] << ')';
					}
					if (per->NumberParameters > 1)
						logger << " 0x" << hex << setw(8) << setfill('0') << per->ExceptionInformation[1];
					logger << endl;
				}
				break;
			case EXCEPTION_IN_PAGE_ERROR:
				if (per->NumberParameters > 0)
				{
					logger << "Cause: ";
					switch (per->ExceptionInformation[0])
					{
					case 0:
						logger << "read";
						break;
					case 1:
						logger << "write";
						break;
					case 8:
						logger << "dep";
						break;
					default:
						logger << "flag(" << per->ExceptionInformation[0] << ')';
					}
					if (per->NumberParameters > 1)
					{
						logger << " 0x" << hex << setw(8) << setfill('0') << per->ExceptionInformation[1];

						logger << " NSTATUS = " << per->ExceptionInformation[2];
					}

					logger << endl;
				}
				break;
			}
		}

//#ifdef PLATFORM_X86
		logger << "Context: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->ContextFlags << std::endl;
		logger << "CPU:" << std::endl;
		logger << '\t' << "EAX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Eax;
		logger << '\t' << "EBX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Ebx;
		logger << '\t' << "ECX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Ecx;
		logger << '\t' << "EDX: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Edx;
		logger << std::endl;
		logger << '\t' << "ESI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Esi;
		logger << '\t' << "EDI: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Edi;
		logger << '\t' << "ESP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Esp;
		logger << '\t' << "EBP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Ebp;
		logger << std::endl << std::endl;
		logger << '\t' << "EIP: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Eip;
		logger << '\t' << "EFLAGS: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->EFlags;
		logger << std::endl << std::endl;
		logger << '\t' << "CS: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegCs;
		logger << '\t' << "DS: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegDs;
		logger << '\t' << "ES: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegEs;
		logger << '\t' << "SS: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegSs;
		logger << std::endl;
		logger << '\t' << "FS: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegFs;
		logger << '\t' << "GS: 0x" << std::hex << std::setw(4) << std::setfill('0') << pep->ContextRecord->SegGs;
		logger << std::endl << std::endl;
		logger << '\t' << "DR0: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr0;
		logger << '\t' << "DR1: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr1;
		logger << '\t' << "DR2: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr2;
		logger << '\t' << "DR3: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr3;
		logger << std::endl;
		logger << '\t' << "DR6: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr6;
		logger << '\t' << "DR7: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->Dr7;
		logger << std::endl << std::endl;
		logger << "FPU:" << std::endl;
		logger << '\t' << "ControlWord: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.ControlWord << std::endl;
		logger << '\t' << "StatusWord: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.StatusWord << std::endl;
		logger << '\t' << "TagWord: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.TagWord << std::endl;
		logger << '\t' << "ErrorOffset: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.ErrorOffset << std::endl;
		logger << '\t' << "ErrorSelector: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.ErrorSelector << std::endl;
		logger << '\t' << "DataOffset: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.DataOffset << std::endl;
		logger << '\t' << "DataSelector: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.DataSelector << std::endl;
//		logger << '\t' << "Cr0NpxState: 0x" << std::hex << std::setw(8) << std::setfill('0') << pep->ContextRecord->FloatSave.Cr0NpxState << std::endl;

		unsigned i = 0;
		unsigned char ch;
		/*
		for (unsigned j = 0; j < 10; ++j)
		{
			logger << '\t' << "ST(" << j << "): ";
			for (; i < (j + 1) * 10; ++i)
			{
				ch = pep->ContextRecord->FloatSave.RegisterArea[i];
				logger << hex_nums[ch >> 4] << hex_nums[ch & 0xf] << ' ';
			}
			logger << std::endl;
		}

		logger << std::endl;
//		logger.flush();
		*/

		char const * p = (char const *)pep->ContextRecord->Eip;
		char const * end = (char const *)(pep->ContextRecord->Eip + 4096 - pep->ContextRecord->Eip % 4096);
		if (end > p + 256)
			end = p + 256;

		{
			auto & l = (logger << "Bytes at EIP:\r\n\t");

			for (i = 0; p < end; ++p, ++i)
			{
				ch = *p;
				l << hex_nums[ch >> 4] << hex_nums[ch & 0xf] << ' ';
				if (i == 7)
					l << "  ";
				else if (i == 15)
				{
					l << "\r\n\t";
					i = ~0;
				}
			}
		}


		logger << std::dec;

//		CreateMiniDump(pep);

		//do_walk();
//#endif

		std::cerr.flush();
		std::cout.flush();
		std::clog.flush();

		//        exit(1);
	}

	if (!cont)
		on_crash();

	return cont ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

bool WINAPI ConsoleHandler(unsigned ctrlType)
{
	std::cerr.flush();
	std::cout.flush();
	std::clog.flush();
	return true;
}
#else
#include <sys/time.h>

#endif

namespace boost
{

	void assertion_failed(char const * expr, char const * function, char const * file, long line)
	{
		_throw_simple_error(file, (signed)line, function, expr);
	}

}


unsigned console_height = 25;
unsigned console_width = 80;

#ifndef __GNUC__
std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf8_16_conversion;
#else
using boost::locale::conv::utf_to_utf;
#endif

std::wstring utf8_to_utf16(const char * str)
{
#ifndef __GNUC__
	return utf8_16_conversion.from_bytes(str);
#else
	return utf_to_utf<wchar_t>(str, str + strlen(str));
#endif
}

#ifdef WIN32
long long perf_freq = 0;
#endif

void init_platform_specific(char const * const title)
{
#ifdef _WIN32
	{
		LARGE_INTEGER a;
		if (QueryPerformanceFrequency(&a))
		{
			perf_freq = a.QuadPart;
			long long seconds = 0x7fffffffffffffffLL / 1000 / perf_freq;
			logger << LIGHT_RED << "Timer: " << ((double)seconds / (60 * 60 * 24)) << " days" << std::hex << " 0x" << seconds << std::dec << std::endl;
		}
	}
#ifndef _DEBUG
	HWND hConsole = GetConsoleWindow();
	if (hConsole != 0)
	{
		//greys the X button on the window
		//EnableMenuItem(GetSystemMenu(hConsole, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
		//RemoveMenu(GetSystemMenu(hConsole, FALSE), SC_CLOSE, MF_BYCOMMAND);
	}
#endif
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)&ConsoleHandler, true);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	SetUnhandledExceptionFilter(ExceptionFilter);
//#ifdef __MINGW32__
	SetConsoleTitle(title);
//#else
//	SetConsoleTitle(utf8_to_utf16(title).c_str());
//#endif

	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout != INVALID_HANDLE_VALUE)
	{
		if (GetConsoleScreenBufferInfo(hStdout, &csbiInfo))
		{
			console_width = csbiInfo.dwSize.X;
			console_height = csbiInfo.dwSize.Y;
			logger.set_input_width(console_width - 1);
		}
	}
#endif

//#ifdef _DEBUG
//	std::atomic<unsigned> v = 0;
//	logger << "atomic<unsigned> lockfree = " << (v.is_lock_free() ? "True" : "False") << endl;
//#endif
}

unsigned get_tick() NOEXCEPT
{
#ifdef WIN32
	return GetTickCount();
	/*
	LARGE_INTEGER a;
	if (!QueryPerformanceCounter(&a))
		return 0;

	return a.QuadPart * 1000 / perf_freq;
	*/
#else
    struct timeval tv;
    if(gettimeofday(&tv, NULL) != 0)
        return 0;

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}


const char * const ansi_console_color_codes[16] =
{
	"\033[22;30m",
	"\033[22;34m",
	"\033[22;32m",
	"\033[22;36m",
	"\033[22;31m",
	"\033[22;35m",
	"\033[22;33m",
	"\033[22;37m",
	"\033[01;30m",
	"\033[01;34m",
	"\033[01;32m",
	"\033[01;36m",
	"\033[01;31m",
	"\033[01;35m",
	"\033[01;33m",
	"\033[01;37m"
};

void set_console_color(int color) NOEXCEPT
{
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
#else
	if (color < 16 && color >= 0)
		std::cout << ansi_console_color_codes[color];
#endif
}

void clear_screen()
{
#ifdef _WIN32
	system("cls");
#else
	std::cout << "\033[2J\033[1;1H";
#endif
}


bool pp_kbhit() NOEXCEPT
{
#ifdef _WIN32
	return ::_kbhit() != 0;
#else
	timeval TO{ 0, 0 };

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);

	return select(0 + 1, &fds, 0, 0, &TO) > 0;
#endif
}

int pp_getch() NOEXCEPT
{
#ifdef _WIN32
	return ::_getch();
#else
	char ch;
	if (read(0, &ch, 1) != 1)
		return 0;
	return ch;
#endif
}




void syslog_info(std::string const & str)
{
#ifdef AS_DAEMON
	syslog(LOG_INFO, "%s", str.c_str());
#endif
}

void syslog_warning(std::string const & str)
{
#ifdef AS_DAEMON
	syslog(LOG_WARNING, "%s", str.c_str());
#endif
}

void syslog_error(std::string const & str)
{
#ifdef AS_DAEMON
	syslog(LOG_ERR, "%s", str.c_str());
#endif
}


