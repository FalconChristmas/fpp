#ifndef __FPP_PCH__
#define __FPP_PCH__

// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64

#include <vector>
#include <array>
#include <list>
#include <map>
#include <set>
#include <string>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <atomic>
#include <cmath>

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>

#include <jsoncpp/json/json.h>
#include <httpserver.hpp>

#include "common.h"
#include "settings.h"
#include "Sequence.h"
#include "fppversion.h"
#include "log.h"
#include "mqtt.h"
#include "commands/Commands.h"
#include "Warnings.h"

#endif
