/*
 *   RemapOutputProcessor class for Falcon Player (FPP)
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRIGHTNESSOUTPUTPROCESSOR_H
#define _BRIGHTNESSOUTPUTPROCESSOR_H

#include "OutputProcessor.h"

class BrightnessOutputProcessor : public OutputProcessor {
public:
    BrightnessOutputProcessor(const Json::Value &config);
    virtual ~BrightnessOutputProcessor();
    
    virtual void ProcessData(unsigned char *channelData) const;
    
    virtual OutputProcessorType getType() const { return BRIGHTNESS; }

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
        addRange(start, start + count - 1);
    }

protected:
    int start;
    int count;
    int brightness;
    float gamma;
    unsigned char table[256];
};

#endif
