/*
 *   Light-O-Rama (LOR) channel output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2019 the Falcon Player Developers
 *      For additional credits and developers, see credits.php.
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

#ifndef _LOR_H
#define _LOR_H

#include "ChannelOutputBase.h"

class LOROutputData;

class LOROutput : public ChannelOutputBase {
    public:
    LOROutput(unsigned int startChannel, unsigned int channelCount);
    ~LOROutput();
    
    virtual int Init(Json::Value config);
    int Init(char *configStr);
    
    int Close(void);
    
    int SendData(unsigned char *channelData);
    
    void DumpConfig(void);
    
    virtual void GetRequiredChannelRange(int &min, int & max);
    
    private:
    LOROutputData *data;
};


#endif /* _LOR_H */
