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
    if (colorOrderStr == "RGBW")
        return FPPColorOrder::kColorOrderRGBW;
    if (colorOrderStr == "RBGW")
        return FPPColorOrder::kColorOrderRBGW;
    if (colorOrderStr == "GRBW")
        return FPPColorOrder::kColorOrderGRBW;
    if (colorOrderStr == "GBRW")
        return FPPColorOrder::kColorOrderGBRW;
    if (colorOrderStr == "BRGW")
        return FPPColorOrder::kColorOrderBRGW;
    if (colorOrderStr == "BGRW")
        return FPPColorOrder::kColorOrderBGRW;
    if (colorOrderStr == "WRGB")
        return FPPColorOrder::kColorOrderWRGB;
    if (colorOrderStr == "WRBG")
        return FPPColorOrder::kColorOrderWRBG;
    if (colorOrderStr == "WGRB")
        return FPPColorOrder::kColorOrderWGRB;
    if (colorOrderStr == "WGBR")
        return FPPColorOrder::kColorOrderWGBR;
    if (colorOrderStr == "WBRG")
        return FPPColorOrder::kColorOrderWBRG;
    if (colorOrderStr == "WBGR")
        return FPPColorOrder::kColorOrderWBGR;
    return FPPColorOrder::kColorOrderRGB;
}

const std::string ColorOrderToString(const FPPColorOrder& colorOrder) {
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
    case FPPColorOrder::kColorOrderRGBW:
        return std::string("RGBW");
    case FPPColorOrder::kColorOrderRBGW:
        return std::string("RBGW");
    case FPPColorOrder::kColorOrderGRBW:
        return std::string("GRBW");
    case FPPColorOrder::kColorOrderGBRW:
        return std::string("GBRW");
    case FPPColorOrder::kColorOrderBRGW:
        return std::string("BRGW");
    case FPPColorOrder::kColorOrderBGRW:
        return std::string("BGRW");
    case FPPColorOrder::kColorOrderWRGB:
        return std::string("WRGB");
    case FPPColorOrder::kColorOrderWRBG:
        return std::string("WRBG");
    case FPPColorOrder::kColorOrderWGRB:
        return std::string("WGRB");
    case FPPColorOrder::kColorOrderWGBR:
        return std::string("WGBR");
    case FPPColorOrder::kColorOrderWBRG:
        return std::string("WBRG");
    case FPPColorOrder::kColorOrderWBGR:
        return std::string("WBGR");
    }
    return std::string("UNKNOWN");
}

int FPPColorOrder::redOffset() const {
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
    case FPPColorOrder::kColorOrderRGBW:
    case FPPColorOrder::kColorOrderRBGW:
        return 0;
    case FPPColorOrder::kColorOrderGRBW:
    case FPPColorOrder::kColorOrderBRGW:
        return 1;
    case FPPColorOrder::kColorOrderGBRW:
    case FPPColorOrder::kColorOrderBGRW:
        return 2;
    case FPPColorOrder::kColorOrderWRGB:
    case FPPColorOrder::kColorOrderWRBG:
        return 1;
    case FPPColorOrder::kColorOrderWGRB:
    case FPPColorOrder::kColorOrderWBRG:
        return 2;
    case FPPColorOrder::kColorOrderWGBR:
    case FPPColorOrder::kColorOrderWBGR:
        return 3;
    }
    return 0;
}
int FPPColorOrder::greenOffset() const {
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
    case FPPColorOrder::kColorOrderGRBW:
    case FPPColorOrder::kColorOrderGBRW:
        return 0;
    case FPPColorOrder::kColorOrderRGBW:
    case FPPColorOrder::kColorOrderBGRW:
        return 1;
    case FPPColorOrder::kColorOrderBRGW:
    case FPPColorOrder::kColorOrderRBGW:
        return 2;
    case FPPColorOrder::kColorOrderWGRB:
    case FPPColorOrder::kColorOrderWGBR:
        return 1;
    case FPPColorOrder::kColorOrderWRGB:
    case FPPColorOrder::kColorOrderWBGR:
        return 2;
    case FPPColorOrder::kColorOrderWBRG:
    case FPPColorOrder::kColorOrderWRBG:
        return 3;
    }
    return 0;
}
int FPPColorOrder::blueOffset() const {
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
    case FPPColorOrder::kColorOrderBRGW:
    case FPPColorOrder::kColorOrderBGRW:
        return 0;
    case FPPColorOrder::kColorOrderGBRW:
    case FPPColorOrder::kColorOrderRBGW:
        return 1;
    case FPPColorOrder::kColorOrderRGBW:
    case FPPColorOrder::kColorOrderGRBW:
        return 2;
    case FPPColorOrder::kColorOrderWBRG:
    case FPPColorOrder::kColorOrderWBGR:
        return 1;
    case FPPColorOrder::kColorOrderWGBR:
    case FPPColorOrder::kColorOrderWRBG:
        return 2;
    case FPPColorOrder::kColorOrderWRGB:
    case FPPColorOrder::kColorOrderWGRB:
        return 3;
    }
    return 0;
}

int FPPColorOrder::whiteOffset() const {
    switch (value) {
    case FPPColorOrder::kColorOrderRGBW:
    case FPPColorOrder::kColorOrderRBGW:
    case FPPColorOrder::kColorOrderGRBW:
    case FPPColorOrder::kColorOrderGBRW:
    case FPPColorOrder::kColorOrderBRGW:
    case FPPColorOrder::kColorOrderBGRW:
        return 3;
    case FPPColorOrder::kColorOrderWRGB:
    case FPPColorOrder::kColorOrderWRBG:
    case FPPColorOrder::kColorOrderWGRB:
    case FPPColorOrder::kColorOrderWGBR:
    case FPPColorOrder::kColorOrderWBRG:
    case FPPColorOrder::kColorOrderWBGR:
        return 0;
    }
    return -1;
}

int FPPColorOrder::channelCount() const {
    switch (value) {
    case FPPColorOrder::kColorOrderRGB:
    case FPPColorOrder::kColorOrderRBG:
    case FPPColorOrder::kColorOrderGRB:
    case FPPColorOrder::kColorOrderGBR:
    case FPPColorOrder::kColorOrderBRG:
    case FPPColorOrder::kColorOrderBGR:
        return 3;
    case FPPColorOrder::kColorOrderONE:
        return 1;
    case FPPColorOrder::kColorOrderRGBW:
    case FPPColorOrder::kColorOrderRBGW:
    case FPPColorOrder::kColorOrderGRBW:
    case FPPColorOrder::kColorOrderGBRW:
    case FPPColorOrder::kColorOrderBRGW:
    case FPPColorOrder::kColorOrderBGRW:
    case FPPColorOrder::kColorOrderWRGB:
    case FPPColorOrder::kColorOrderWRBG:
    case FPPColorOrder::kColorOrderWGRB:
    case FPPColorOrder::kColorOrderWGBR:
    case FPPColorOrder::kColorOrderWBRG:
    case FPPColorOrder::kColorOrderWBGR:
        return 4;
    }
    return 3;
}

bool FPPColorOrder::hasWhite() const {
    return channelCount() == 4;
}
