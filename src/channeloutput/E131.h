/*
 *   E131 output handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
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

#ifndef _E131_H
#define _E131_H

#include <sys/uio.h>
#include <netinet/in.h>

#include "UDPOutput.h"
#include "e131defs.h"

class E131OutputData : public UDPOutputData {
public:
    E131OutputData(const Json::Value &config);
    virtual ~E131OutputData();
    
    virtual bool IsPingable();
    virtual void PrepareData(unsigned char *channelData);
    virtual void CreateMessages(std::vector<struct mmsghdr> &ipMsgs);
    virtual void DumpConfig();

    int           universe;
    int           priority;
    char          E131sequenceNumber;

    sockaddr_in   e131Address;
    struct iovec  e131Iovecs[2];
    unsigned char e131Buffer[E131_HEADER_LENGTH];
};

#endif
