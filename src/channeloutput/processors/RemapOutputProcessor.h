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
    RemapOutputProcessor(int src, int dst, int count, int loop);
    virtual ~RemapOutputProcessor();
    
    virtual void ProcessData(unsigned char *channelData) const;
    
    virtual OutputProcessorType getType() const { return REMAP; }

    int getSourceChannel() const { return sourceChannel;}
    int getDestChannel() const { return destChannel;}
    int getCount() const { return count;}
    int getLoops() const { return loops;}
protected:
    int sourceChannel;
    int destChannel;
    int count;
    int loops;
};

#endif
