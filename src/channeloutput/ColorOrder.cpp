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

#include "ColorOrder.h"

FPPColorOrder ColorOrderFromString(const std::string& colorOrderStr) {
    if (colorOrderStr == "RGB")
        return kColorOrderRGB;

    if (colorOrderStr == "RBG")
        return kColorOrderRBG;

    if (colorOrderStr == "GRB")
        return kColorOrderGRB;

    if (colorOrderStr == "GBR")
        return kColorOrderGBR;

    if (colorOrderStr == "BRG")
        return kColorOrderBRG;

    if (colorOrderStr == "BGR")
        return kColorOrderBGR;

    if (colorOrderStr == "W")
        return kColorOrderONE;

    return kColorOrderRGB;
}

const std::string ColorOrderToString(FPPColorOrder colorOrder) {
    switch (colorOrder) {
    case kColorOrderRGB:
        return std::string("RGB");
    case kColorOrderRBG:
        return std::string("RBG");
    case kColorOrderGBR:
        return std::string("GBR");
    case kColorOrderGRB:
        return std::string("GRB");
    case kColorOrderBGR:
        return std::string("BGR");
    case kColorOrderBRG:
        return std::string("BRG");
    case kColorOrderONE:
        return std::string("W");
    }

    return std::string("UNKNOWN");
}
