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
        return FPPColorOrder::kColorOrderRGB;

    if (colorOrderStr == "RBG")
        return FPPColorOrder::kColorOrderRBG;

    if (colorOrderStr == "GRB")
        return FPPColorOrder::kColorOrderGRB;

    if (colorOrderStr == "GBR")
        return FPPColorOrder::kColorOrderGBR;

    if (colorOrderStr == "BRG")
        return FPPColorOrder::kColorOrderBRG;

    if (colorOrderStr == "BGR")
        return FPPColorOrder::kColorOrderBGR;

    if (colorOrderStr == "W")
        return FPPColorOrder::kColorOrderONE;

    return FPPColorOrder::kColorOrderRGB;
}

const std::string ColorOrderToString(FPPColorOrder colorOrder) {
    switch (colorOrder) {
    case FPPColorOrder::kColorOrderRGB:
        return std::string("RGB");
    case FPPColorOrder::kColorOrderRBG:
        return std::string("RBG");
    case FPPColorOrder::kColorOrderGBR:
        return std::string("GBR");
    case FPPColorOrder::kColorOrderGRB:
        return std::string("GRB");
    case FPPColorOrder::kColorOrderBGR:
        return std::string("BGR");
    case FPPColorOrder::kColorOrderBRG:
        return std::string("BRG");
    case FPPColorOrder::kColorOrderONE:
        return std::string("W");
    }

    return std::string("UNKNOWN");
}


int FPPColorOrder::redOffset() {
    switch (value) {
        case FPPColorOrder::kColorOrderONE:
        case FPPColorOrder::kColorOrderRGB:
        case FPPColorOrder::kColorOrderRBG:
            return 0;
        case FPPColorOrder::kColorOrderGRB:
        case FPPColorOrder::kColorOrderBRG:
            return 1;
        case FPPColorOrder::kColorOrderGBR:
        case FPPColorOrder::kColorOrderBGR:
            return 2;
    }
    return 0;
}
int FPPColorOrder::greenOffset() {
    switch (value) {
        case FPPColorOrder::kColorOrderONE:
        case FPPColorOrder::kColorOrderGRB:
        case FPPColorOrder::kColorOrderGBR:
            return 0;
        case FPPColorOrder::kColorOrderRGB:
        case FPPColorOrder::kColorOrderBGR:
            return 1;
        case FPPColorOrder::kColorOrderBRG:
        case FPPColorOrder::kColorOrderRBG:
            return 2;
    }
    return 0;
}
int FPPColorOrder::blueOffset() {
    switch (value) {
        case FPPColorOrder::kColorOrderONE:
        case FPPColorOrder::kColorOrderBRG:
        case FPPColorOrder::kColorOrderBGR:
            return 0;
        case FPPColorOrder::kColorOrderGBR:
        case FPPColorOrder::kColorOrderRBG:
            return 1;
        case FPPColorOrder::kColorOrderRGB:
        case FPPColorOrder::kColorOrderGRB:
            return 2;
    }
    return 0;
}
