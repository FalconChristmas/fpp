#pragma once
/*
 *   MQTT Output handler for Falcon Player (FPP)
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

#include <string>

#include "ThreadedChannelOutputBase.h"

class MQTTOutput : public ThreadedChannelOutputBase {
public:
    MQTTOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~MQTTOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    enum class OutputType {
        ONE_CHAN = 1,
        THREE_CHAN = 3,
        FOUR_CHAN = 4
    };

    std::string m_topic;
    std::string m_payload;
    OutputType m_type;
    uint8_t m_r;
    uint8_t m_g;
    uint8_t m_b;
    uint8_t m_w;

    void ReplaceValues(std::string& valueStr, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void RGBtoHSIV(float fR, float fG, float fB, float& fH, float& fSI, float& fSV, float& fI, float& fV);
};
