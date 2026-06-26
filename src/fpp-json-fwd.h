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

// Forward declaration of Json::Value (the <iosfwd> idiom). A header that only
// *names* Json::Value in declarations -- method parameters/returns, references,
// pointers -- should include this instead of "fpp-json.h" so it does not drag
// the (large) jsoncpp header into every translation unit that includes it. The
// .cpp that defines/calls those methods includes "fpp-json.h" for the complete
// type.
//
// Headers that need the complete type (a by-value Json::Value member, a base
// class, an inline body that calls Json::Value, a default argument that
// constructs one, or any Json:: enum/typedef such as Json::arrayValue /
// Json::ValueType / Json::UInt) must include "fpp-json.h" instead.

namespace Json {
class Value;
}
