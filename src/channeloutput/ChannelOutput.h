#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <functional>
#include <string>
#include <vector>

class ChannelOutput {
public:
    ChannelOutput(unsigned int startChannel = 1,
                  unsigned int channelCount = 1);
    virtual ~ChannelOutput();

    virtual std::string GetOutputType() const {
        return typeid(*this).name();
    }

    unsigned int ChannelCount(void) { return m_channelCount; }
    unsigned int StartChannel(void) { return m_startChannel; }

    virtual int Init(Json::Value config);
    virtual int Close(void);

    virtual void PrepData(unsigned char* channelData) {}
    virtual int SendData(unsigned char* channelData) = 0;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) = 0;

    // Some outputs may need to know ahead of time that they are about to start or stop outputting
    // data so that resources can be setup that are only valid during the time the data is
    // being output.  For example, tcp sockets or auth tokens or similar.
    virtual void StartingOutput() {}
    virtual void StoppingOutput() {}

    // For "OutputSpecific" test types, the output can overlay data based on the configuration
    // it knows (per port, per universe, per panel, etc...).   TestType is specific to the
    // output as well and could be a cycle color, output pattern, etc....
    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {}
    virtual bool SupportsTesting() const { return false; }

protected:
    virtual void DumpConfig(void);
    virtual void ConvertToCSV(Json::Value config, char* configStr);

    std::string m_outputType;
    unsigned int m_startChannel;
    unsigned int m_channelCount;
};
