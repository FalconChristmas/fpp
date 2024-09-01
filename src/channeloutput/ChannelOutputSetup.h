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

#include <pthread.h>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

class ChannelOutput;
class OutputProcessors;

typedef struct fppChannelOutput {
    int (*maxChannels)(void* data);
    int (*open)(const char* device, void** privDataPtr);
    int (*close)(void* data);
    int (*isConfigured)(void);
    int (*isActive)(void* data);
    int (*send)(void* data, const char* channelData, int channelCount);
    int (*startThread)(void* data);
    int (*stopThread)(void* data);
} FPPChannelOutput;

class FPPChannelOutputInstance {
public:
    FPPChannelOutputInstance() {}
    ~FPPChannelOutputInstance() {}

    unsigned int startChannel = 0;
    unsigned int channelCount = 0;
    FPPChannelOutput* outputOld = nullptr;
    ChannelOutput* output = nullptr;
    void* privData = nullptr;
};

extern char channelData[];
extern pthread_mutex_t channelDataLock;
extern unsigned long channelOutputFrame;
extern float mediaElapsedSeconds;
extern OutputProcessors outputProcessors;

int InitializeChannelOutputs(void);
int PrepareChannelData(char* channelData);
int SendChannelData(const char* channelData);
void OverlayOutputTestData(std::set<std::string> types, unsigned char* channelData, int cycleCnt, float percentOfCycle, int testType, const Json::Value& extraConfig);
std::set<std::string> GetOutputTypes();
void CloseChannelOutputs(void);
void SetChannelOutputFrameNumber(int frameNumber);
void ResetChannelOutputFrameNumber(void);

void StartingOutput(void);
void StoppingOutput(void);

const std::vector<std::pair<uint32_t, uint32_t>>& GetOutputRanges(bool precise = true);
std::string GetOutputRangesAsString(bool precise = true, bool oneBased = false);
