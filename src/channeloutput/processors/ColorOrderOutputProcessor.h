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

#ifndef _COLORORDEROUTPUTPROCESSOR_H
#define _COLORORDEROUTPUTPROCESSOR_H

#include "OutputProcessor.h"

class ColorOrderOutputProcessor : public OutputProcessor {
public:
    ColorOrderOutputProcessor(const Json::Value &config);
    virtual ~ColorOrderOutputProcessor();
    
    virtual void ProcessData(unsigned char *channelData) const;
    
    virtual OutputProcessorType getType() const { return COLORORDER; }

    virtual void GetRequiredChannelRange(int &min, int &max) {
        min = start;
        max = start + (count * 3) - 1;
    }

protected:
    int start;
    int count;
    int order;
};

#endif
