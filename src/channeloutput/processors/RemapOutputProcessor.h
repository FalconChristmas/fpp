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

#ifndef _REMAPOUTPUTPROCESSOR_H
#define _REMAPOUTPUTPROCESSOR_H

#include "OutputProcessor.h"

class RemapOutputProcessor : public OutputProcessor {
public:
    RemapOutputProcessor(const Json::Value &config);
    RemapOutputProcessor(int src, int dst, int count, int loop, int reverse);
    virtual ~RemapOutputProcessor();
    
    virtual void ProcessData(unsigned char *channelData) const override;
    
    virtual OutputProcessorType getType() const override { return REMAP; }

    int getSourceChannel() const { return sourceChannel;}
    int getDestChannel() const { return destChannel;}
    int getCount() const { return count;}
    int getLoops() const { return loops;}
    int getReverse() const { return reverse;}
    
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

protected:
    int sourceChannel;
    int destChannel;
    int count;
    int loops;
    int reverse;
};

#endif
