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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define FASTLED_USING_NAMESPACE
#define FL_ALIGN_PROGMEM
#define FL_PROGMEM
#define FL_PGM_READ_BYTE_NEAR(x)  (*((const  uint8_t*)(x)))
#define FL_PGM_READ_WORD_NEAR(x)  (*((const uint16_t*)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const uint32_t*)(x)))


#include "pixeltypes.h"
#include "lib8tion.h"
#include "colorutils.h"

class FakeFL {
public:
    void clear() {};
};
extern FakeFL FastLED;

