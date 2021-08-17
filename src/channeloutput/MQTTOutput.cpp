/*
 *    MQTT Output handler for Falcon Player (FPP)
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

#include "fpp-pch.h"

#include "mqtt.h"

#include "MQTTOutput.h"

/////////////////////////////////////////////////////////////////////////////
extern "C" {
MQTTOutput* createMQTTOutputOutput(unsigned int startChannel,
                                   unsigned int channelCount) {
    return new MQTTOutput(startChannel, channelCount);
}
}
/*
 *
 */
MQTTOutput::MQTTOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutputBase(startChannel, channelCount),
    m_payload("%R%,%G%,%B%"),
    m_type(OutputType::THREE_CHAN),
    m_r(0x00),
    m_g(0x00),
    m_b(0x00),
    m_w(0x00) {
    LogDebug(VB_CHANNELOUT, "MQTTOutput::MQTTOutput(%u, %u)\n",
             startChannel, channelCount);

    m_useDoubleBuffer = 1;
}

/*
 *
 */
MQTTOutput::~MQTTOutput() {
    LogDebug(VB_CHANNELOUT, "MQTTOutput::~MQTTOutput()\n");
}

int MQTTOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "MQTTOutput::Init()\n");

    if (!mqtt) {
        LogWarn(VB_CHANNELOUT, "MQTT Not Configured, cannot start MQTT Output");
        return false;
    }

    if (config.isMember("topic")) {
        m_topic = config["topic"].asString();
    }
    if (config.isMember("payload")) {
        auto payload = config["payload"].asString();
        if (!payload.empty()) {
            m_payload = payload;
        }
    }
    if (config.isMember("channelType")) {
        auto type = config["channelType"].asString();
        if (type == "RGBW") {
            m_type = OutputType::FOUR_CHAN;
        } else if (type == "RGB") {
            m_type = OutputType::THREE_CHAN;
        } else if (type == "Single") {
            m_type = OutputType::ONE_CHAN;
        }
    }

    return ThreadedChannelOutputBase::Init(config);
}

void MQTTOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + (int)m_type - 1);
}

/*
 *
 */
int MQTTOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "MQTTOutput::Close()\n");

    return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int MQTTOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "MQTTOutput::RawSendData(%p)\n", channelData);

    if (!mqtt || !mqtt->IsConnected()) {
        return 0;
    }
    if (m_topic.empty()) {
        return 0;
    }
    if (m_payload.empty()) {
        return 0;
    }

    uint8_t r = 0x00;
    uint8_t g = 0x00;
    uint8_t b = 0x00;
    uint8_t w = 0x00;

    //The double buffer that ThreadedOutputBase performs only copies/buffers
    //data from the startChannel for the required length.   The channelData
    //passed in is already starting at startChannel
    switch (m_type) {
    case OutputType::ONE_CHAN:
        w = channelData[0];
        break;
    case OutputType::FOUR_CHAN:
        w = channelData[3];
    case OutputType::THREE_CHAN:
        r = channelData[0];
        g = channelData[1];
        b = channelData[2];
        break;
    }

    if (r == m_r && g == m_g && b == m_b && w == m_w) {
        return m_channelCount;
    }

    m_r = r;
    m_g = g;
    m_b = b;
    m_w = w;

    std::string payload = m_payload;

    ReplaceValues(payload, r, g, b, w);

    mqtt->PublishRaw(m_topic, payload);

    return m_channelCount;
}

/*
 *
 */
void MQTTOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "MQTTOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Topic           : %s\n", m_topic.c_str());
    LogDebug(VB_CHANNELOUT, "    Payload         : %s\n", m_topic.c_str());
    LogDebug(VB_CHANNELOUT, "    Type            : %d\n", (int)m_type);

    ThreadedChannelOutputBase::DumpConfig();
}

void MQTTOutput::ReplaceValues(std::string& valueStr, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    replaceAll(valueStr, "%R%", std::to_string(r));
    replaceAll(valueStr, "%G%", std::to_string(g));
    replaceAll(valueStr, "%B%", std::to_string(b));
    replaceAll(valueStr, "%W%", std::to_string(w));

    int rScale = (r * 100) / 255;
    int gScale = (g * 100) / 255;
    int bScale = (b * 100) / 255;
    int wScale = (w * 100) / 255;

    replaceAll(valueStr, "%RS%", std::to_string(rScale));
    replaceAll(valueStr, "%GS%", std::to_string(gScale));
    replaceAll(valueStr, "%BS%", std::to_string(bScale));
    replaceAll(valueStr, "%WS%", std::to_string(wScale));

    if (r > 254 || g > 254 || b > 254 || w > 254) {
        replaceAll(valueStr, "%SW%", "ON");
    } else {
        replaceAll(valueStr, "%SW%", "OFF");
    }

    float h, si, sv, i, v;

    RGBtoHSIV(r / 255, g / 255, b / 255, h, si, sv, i, v);

    int ih = (h);
    int isi = (si * 100);
    int isv = (sv * 100);
    int ii = (i * 100);
    int iv = (v * 100);

    replaceAll(valueStr, "%H%", std::to_string(ih));
    replaceAll(valueStr, "%S%", std::to_string(isi));
    replaceAll(valueStr, "%SI%", std::to_string(isi));
    replaceAll(valueStr, "%SV%", std::to_string(isv));
    replaceAll(valueStr, "%I%", std::to_string(ii));
    replaceAll(valueStr, "%V%", std::to_string(iv));
}

void MQTTOutput::RGBtoHSIV(float fR, float fG, float fB, float& fH, float& fSI, float& fSV, float& fI, float& fV) {
    float M = std::max(std::max(fR, fG), fB);
    float m = std::min(std::min(fR, fG), fB);
    float c = M - m;
    fV = M;
    //fL = (1.0/2.0)*(M+m);
    fI = (1.0 / 3.0) * (fR + fG + fB);

    if (c == 0) {
        fH = 0.0;
        fSI = 0.0;
    } else {
        if (M == fR) {
            fH = fmod(((fG - fB) / c), 6.0);
        } else if (M == fG) {
            fH = (fB - fR) / c + 2.0;
        } else if (M == fB) {
            fH = (fR - fG) / c + 4.0;
        }
        fH *= 60.0;
        if (fI != 0) {
            fSI = 1.0 - (m / fI);
        } else {
            fSI = 0.0;
        }
    }

    fSV = M == 0 ? 0 : (M - m) / M;

    if (fH < 0.0)
        fH += 360.0;
}
