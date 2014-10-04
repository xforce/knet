/*
* Copyright (C) 2014 Crix-Dev
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Crix-Dev nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// TODO: change include guard to say its logger to prevent guard mess
#ifndef kNet_Common_h
#define kNet_Common_h

#define DEBUG_ACKS 1

#define _ITERATOR_DEBUG_LEVEL 0

#if _WIN64 || __x86_64__ || __ppc64__
#define IS64BIT 1
#else
#define IS64BIT 0
#endif

#if defined(_WIN64) || defined(_WIN32)
#ifdef WIN32
#undef WIN32
#endif
#define WIN32 1
#elif __APPLE__
#define MAC 1
#elif __linux
#define LINUX 1
#elif __unix
#define UNIX 1
#elif __posix
#define POSIX 1
#endif

#if WIN32
#if CRT_ALLOC
#define CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG
#endif
#endif

#if WIN32
#include <yvals.h> // This is needed for making stuff compatible

#if __cplusplus <= 199711L && _HAS_CPP0X == 0
#error "Please use at least Visual Studio 2013. This project was developed using VS2013 and/or GCC 4.8."
#else
// Check if all used features are vailable
#if !defined(_CPPLIB_VER) || _CPPLIB_VER < 610
#error "Use at least VS2013."
#endif

#if !defined(_HAS_DECLTYPE) || _HAS_DECLTYPE < 1
#error "Decltype is required. Use at least VS2013."
#endif

#if !defined(_HAS_INITIALIZER_LISTS) || _HAS_INITIALIZER_LISTS < 1
#error "Initialiizer lists is required. Use at least VS2013."
#endif

//#if !defined(_HAS_REF_QUALIFIER) || _HAS_REF_QUALIFIER < 1
//#error "Please use at least VS2013."
//#endif

#if !defined(_HAS_RVALUE_REFERENCES) || _HAS_RVALUE_REFERENCES < 1
#error "Rvalue References is required. Use at least VS2013."
#endif

#if !defined(_HAS_SCOPED_ENUM) || _HAS_SCOPED_ENUM < 1
#error "Scoped enum is required. Use at least VS2013."
#endif

#if !defined(_HAS_TEMPLATE_ALIAS) || _HAS_TEMPLATE_ALIAS < 1
#error "Template alias is required. Use at least VS2013."
#endif

#if !defined(_HAS_VARIADIC_TEMPLATES) || _HAS_VARIADIC_TEMPLATES < 1
#error "Variadic templates is required. Use at least VS2013."
#endif

#endif
#elif LINUX
#if __GNUC__ < 4 || (__GNUC__ < 4 && __GNUC_MINOR__ < 7) || (__GNUC__ < 4 && __GNUC_MINOR__ < 7 && __GNUC_PATCHLEVEL__ < 2)
#error "Use at least GCC 4.7.2"
#endif 

#if __cplusplus <= 199711L
if !defined(__GXX_EXPERIMENTAL_CXX0X__) || __GXX_EXPERIMENTAL_CXX0X__ != 1
#error "C++11 is required. Try --std=c++0x"
#endif
#endif

#ifndef _NOEXCEPT
#define NOEXCEPT noexcept
#else
#ifdef __clang__
#define NOEXCEPT noexcept
#else
#define NOEXCEPT _NOEXCEPT
#endif
#endif

// at this point we can use C++11

#ifdef WIN32
#include <WinSock2.h> // Just to be sure that there will never be a fucking include guard error
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;

#define sprintf_s(a, b, c) snprintf(a, sizeof(a), b, c)
#endif

// These are C++ standard includes so not platform specific

#pragma region STD
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include <functional>
#include <tuple>
#include <algorithm>
#include <array>
#include <memory>
#pragma endregion

#include <cassert>
#include <cstdint>
#include <cstring>


typedef std::int8_t int8;
typedef std::uint8_t uint8;

typedef std::int16_t int16;
typedef std::uint16_t uint16;

typedef std::int32_t int32;
typedef std::uint32_t uint32;

typedef std::int64_t int64;
typedef std::uint64_t uint64;

#define _INTERFACE class

#ifdef KEKSNL_STATIC
#   define KEKSNL_API
#else
#if defined(KEKSNL_EXPORT) // inside DLL
#   define KEKSNL_API   __declspec(dllexport)
#else // outside DLL
#   define KEKSNL_API   __declspec(dllimport)
#endif  // KEKSNL_EXPORT
#endif

#if ENABLE_LOGGER
#include <ILogger.h>

#if _DEBUG
//#define DEBUG_LOG(x) GetLogger("kNet")->debug(x);
#define DEBUG_LOG(...) GetLogger("kNet")->debug(__VA_ARGS__);
#else
#define DEBUG_LOG(...)
#endif
#else
#define DEBUG_LOG(...) { printf(__VA_ARGS__); printf("\n"); }
#endif

#include "BitStream.h"

#define SYNCED_CLASS_BODY(...)
#define SYNCED_PROPERTY(...)
#define SYNCED_PROPERTY_FUNC(...)

#endif // kNet_Common_h