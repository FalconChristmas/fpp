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

#include <string>


class FPPColorOrder {
public:
    enum Value {
        kColorOrderRGB = 0,
        kColorOrderRBG,
        kColorOrderGRB,
        kColorOrderGBR,
        kColorOrderBRG,
        kColorOrderBGR,
        kColorOrderONE
    };

    FPPColorOrder() = default;
    constexpr FPPColorOrder(Value order) : value(order) { }

    constexpr operator Value() const { return value; }
    explicit operator bool() const = delete;  

    constexpr bool operator==(FPPColorOrder a) const { return value == a.value; }
    constexpr bool operator==(Value a) const { return value == a; }
    constexpr bool operator!=(FPPColorOrder a) const { return value != a.value; }
    constexpr bool operator!=(Value a) const { return value != a; }

    int redOffset();
    int greenOffset();
    int blueOffset();
private:
    Value value;
};

FPPColorOrder ColorOrderFromString(const std::string& colorOrderStr);
const std::string ColorOrderToString(FPPColorOrder colorOrder);
