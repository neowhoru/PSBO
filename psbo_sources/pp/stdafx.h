// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define STD_HAS_THREADS

#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4800)
#endif

#ifdef _WIN32
#define CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <yvals.h>

#ifdef _DEBUG
#ifndef cnew
#define cnew new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
//#define new cnew
#endif
#endif
#endif

#ifndef cnew
#define cnew new
#endif

#ifdef _MSC_VER
#include <SDKDDKVer.h>
#endif

#if defined(__clang__)
#if __has_feature(cxx_noexcept)
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif
#else
#if  defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ * 10 + __GNUC_MINOR__ >= 46
#define NOEXCEPT noexcept
#else
#if defined(_MSC_VER)
#define NOEXCEPT _NOEXCEPT
#else
#define NOEXCEPT
#endif
#endif
#endif

#ifndef _MSC_VER
#define _forceinline inline
#endif

#include <boost/bind.hpp>
#include <boost/asio.hpp>

//#define _USE_MATH_DEFINES
//#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <tchar.h>
//#include <windows.h>

#include <typeinfo>
#include <typeindex>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>

#ifdef STD_HAS_THREADS
#include <thread>
#include <mutex>
typedef std::mutex mutex_t;
typedef std::thread thread_t;
#define impl_lock_guard(l, m) std::lock_guard<std::mutex> l ( m )
#else
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
typedef boost::mutex mutex_t;
typedef boost::thread thread_t;
#define impl_lock_guard(l, m) boost::lock_guard<boost::mutex> l ( m )
#endif
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <ctime>

#include "common.h"

