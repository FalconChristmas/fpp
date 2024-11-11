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

#include "../../Sequence.h"
#include <functional>

class OutputProcessor {
public:
    OutputProcessor();
    virtual ~OutputProcessor();

    virtual void ProcessData(unsigned char* channelData) const = 0;

    bool isActive() { return active; }

    enum OutputProcessorType {
        UNKNOWN,
        REMAP,
        SETVALUE,
        BRIGHTNESS,
        COLORORDER,
        HOLDVALUE,
        THREETOFOUR,
        OVERRIDEZERO,
        FOLD
    };

    virtual OutputProcessorType getType() const { return UNKNOWN; }

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) = 0;

    virtual void GetRequiredChannelRange(int& min, int& max) final {
        min = 0;
        max = FPPD_MAX_CHANNELS;
    }

protected:
    std::string description;
    bool active;
};

class OutputProcessors {
public:
    OutputProcessors();
    ~OutputProcessors();

    void ProcessData(unsigned char* channelData) const;

    void addProcessor(OutputProcessor* p);
    void removeProcessor(OutputProcessor* p);

    OutputProcessor* find(std::function<bool(OutputProcessor*)> f) const;

    void loadFromJSON(const Json::Value& config, bool clear = true);

    void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange);

protected:
    void removeAll();
    OutputProcessor* create(const Json::Value& config);

    mutable std::mutex processorsLock;
    std::list<OutputProcessor*> processors;
};
