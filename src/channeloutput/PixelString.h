#pragma once
/*
 *   Pixel String Class for Falcon Player (FPP)
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

#include <jsoncpp/json/json.h>
#include <cmath>
#include <string>
#include <vector>
#include <stdint.h>

#include "ColorOrder.h"

class VirtualString {
public:
    VirtualString(); 
    VirtualString(int receiverNum);
    ~VirtualString() {};
    
    int channelsPerNode() const;
    
	int            startChannel;
	int            pixelCount;
	int            groupCount;
	int            reverse;
	FPPColorOrder  colorOrder;
	int            nullNodes;
	int            zigZag;
	int            brightness;
	float          gamma;
	uint8_t        brightnessMap[256];
    
    int            whiteOffset;
    int8_t         receiverNum;
    uint8_t        leadInCount;
    uint8_t        toggleCount;
    uint8_t        leadOutCount;
};

class GPIOCommand {
public:
    GPIOCommand(int p, int offset, int tp = 0, int bo = 0) : port(p), channelOffset(offset), type(tp), bitOffset(bo) {}
    ~GPIOCommand() {}
    
    int port;
    int channelOffset;
    int type;  //0 for off, 1 for on
    int bitOffset;
};


class PixelString {
  public:
	PixelString(bool supportsSmartReceivers = false);
	~PixelString();

	int  Init(Json::Value config);
	void DumpConfig(void);

	int               m_portNumber;
	int               m_channelOffset;
	int               m_outputChannels;

	std::vector<VirtualString>  m_virtualStrings;
    std::vector<GPIOCommand>    m_gpioCommands;

	std::vector<int>  m_outputMap;
	uint8_t         **m_brightnessMaps;
    
    bool m_isSmartReceiver;
  private:
	void SetupMap(int vsOffset, const VirtualString &vs);
	void FlipPixels(int offset1, int offset2, int chanCount);
	void DumpMap(const char *msg);
    
    
    int ReadVirtualString(Json::Value &vsc, VirtualString &vs) const;
    void AddVirtualString(const VirtualString &vs);
    void AddNullPixelString();
};
