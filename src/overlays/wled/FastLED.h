#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define FASTLED_USING_NAMESPACE
#define FL_ALIGN_PROGMEM
#define FL_PROGMEM
#define FL_PGM_READ_BYTE_NEAR(x) (*((const uint8_t*)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const uint16_t*)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const uint32_t*)(x)))

#define WLED_DISABLE_FPP

#define DEBUG_PRINT(A)
#define DEBUG_PRINTLN(A)
#define DEBUG_PRINTF(x...)
#define DEBUG_PRINTF_P(x...)

typedef unsigned char byte;
typedef std::string String;

#define __INC_LED_SYSDEFS_H

class Print {
public:
    Print() {};

    void print(const char* str) {
        printf("%s\n", str);
    }
};

#include "colorpalettes.h"
#include "colorutils.h"
#include "crgb.h"
#include "lib8tion.h"
#include "pixeltypes.h"

class FakeFL {
public:
    void clear() {};
};
extern FakeFL FastLED;
