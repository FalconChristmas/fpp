#ifndef __FPP_PCH__
#define __FPP_PCH__

// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#if __has_include(<httpserver/http_arg_value.hpp>)
#define HTTP_RESPONSE_CONST
#else
#define HTTP_RESPONSE_CONST const
#endif

#ifndef NOPCH
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

#include <httpserver.hpp>

#include "Events.h"
#include "Sequence.h"
#include "Warnings.h"
#include "common.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#endif

#endif
