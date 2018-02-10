/*
 *   BeagleBone Black PRU 48-string handler for Falcon Player (FPP)
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

#ifndef _BBB48STRING_H
#define _BBB48STRING_H

#include <string>
#include <vector>

#include "BBBUtils.h"
#include "ChannelOutputBase.h"
#include "PixelString.h"

#define MAX_WS2811_TIMINGS 128

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;
    
    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile unsigned command;
    volatile unsigned response;
    
    uint16_t timings[MAX_WS2811_TIMINGS];
} __attribute__((__packed__)) BBB48StringData;

class BBB48StringOutput : public ChannelOutputBase {
  public:
    BBB48StringOutput(unsigned int startChannel, unsigned int channelCount);
    ~BBB48StringOutput();

    int Init(Json::Value config);
    int Close(void);

    int RawSendData(unsigned char *channelData);

    void DumpConfig(void);

  private:
    void StopPRU(bool wait = true);
    int StartPRU();

    std::string                m_subType;
    int                        m_maxStringLen;
    int                        m_numStrings;
    size_t                     m_frameSize;
    std::vector<PixelString*>  m_strings;
    std::string                m_pruProgram;
    void ApplyPinMap(const int *map);
    int MapPins(void);
    
    uint8_t            *m_lastData;
    uint8_t            *m_curData;
    uint32_t           m_curFrame;
 
    BBBPru             *m_pru;
    BBB48StringData    *m_pruData;
};

#endif /* _BBB48STRING_H */
