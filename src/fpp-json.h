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

// Single include for jsoncpp that hides the platform path difference:
// Debian/Pi/BBB install the header as <jsoncpp/json/json.h>, while Homebrew on
// macOS installs it as <json/json.h>. Any file that uses Json:: should
//
//     #include "fpp-json.h"
//
// instead of repeating the __has_include dance (or relying on it being pulled
// in transitively via fpp-pch.h). jsoncpp drags in a large chunk of the C++
// standard library, so keeping it out of the universal include path (it is no
// longer force-included by fpp-pch.h under NOPCH/distcc builds) is what lets
// individual translation units stay lean.

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif
