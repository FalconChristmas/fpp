#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include <string>

#include "ThreadedChannelOutput.h"

class MQTTOutput : public ThreadedChannelOutput {
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
