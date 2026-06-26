#ifndef __FPP_PCH__
#define __FPP_PCH__

// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64

// PLATFORM_PI64 is primarily set by pi.mk via CFLAGS (so it also reaches
// plugins that don't include this PCH). This block is a defensive fallback
// that fires only if FPP itself is being compiled against this PCH without
// the makefile flag for some reason -- keeps the two signals from drifting.
// Detection via __aarch64__ is what the compiler actually targets, which is
// the correct ABI signal (kernel bitness is irrelevant here).
#if defined(PLATFORM_PI) && defined(__aarch64__) && !defined(PLATFORM_PI64)
#define PLATFORM_PI64
#endif

// Block libhttpserver from loading: FPP now uses drogon, and fpphttp.h provides
// source-level shims in the httpserver:: namespace for plugin compatibility.
// Defining the guard here causes any #include <httpserver.hpp> in plugin code
// to be silently skipped, preventing redefinition conflicts with our shims.
#define SRC_HTTPSERVER_HPP_

// Kept for backward compatibility with external plugins compiled against the
// old libhttpserver-based API. See CLAUDE.md.
#define HTTP_RESPONSE_CONST

#ifndef NOPCH
// jsoncpp pulls in a large chunk of the C++ standard library, so it is no longer
// force-included into every translation unit. Under a normal (PCH) build it is
// still precompiled here; under NOPCH/distcc builds each file that uses Json::
// includes "fpp-json.h" itself. (FPPWarning no longer derives from Json::Value
// -- see Warnings.h -- which is what frees the many files that only touch the
// warnings API from having to preprocess jsoncpp.)
#include "fpp-json.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "Events.h"
#include "Sequence.h"
#include "Warnings.h"
#include "common.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"

#endif

#endif
