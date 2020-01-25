/*
 *   OutputProcessor class for Falcon Player (FPP)
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

#ifndef _OUTPUTPROCESSOR_H
#define _OUTPUTPROCESSOR_H

#include <string>
#include <list>
#include <mutex>
#include <functional>
#include <jsoncpp/json/json.h>

#include "../../Sequence.h"

class OutputProcessor {
public:
    OutputProcessor();
    virtual ~OutputProcessor();
    
    virtual void ProcessData(unsigned char *channelData) const = 0;
    
    bool isActive() { return active; }

    enum OutputProcessorType {
        UNKNOWN, REMAP, SETVALUE, BRIGHTNESS, COLORORDER, HOLDVALUE
    };

    virtual OutputProcessorType getType() const { return UNKNOWN; }
    
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) = 0;
    
    virtual void GetRequiredChannelRange(int &min, int & max) final {
        min = 0; max = FPPD_MAX_CHANNELS;
    }
protected:
    std::string description;
    bool active;
};


class OutputProcessors {
public:
    OutputProcessors();
    ~OutputProcessors();
    
    void ProcessData(unsigned char *channelData) const;
    
    void addProcessor(OutputProcessor*p);
    void removeProcessor(OutputProcessor*p);
    
    OutputProcessor *find(std::function<bool(OutputProcessor*)> f) const;
    
    void loadFromJSON(const Json::Value &config, bool clear = true);
    
    void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange);
protected:
    void removeAll();
    OutputProcessor *create(const Json::Value &config);
    
    mutable std::mutex processorsLock;
    std::list<OutputProcessor*> processors;
};

#endif /* #ifndef _OUTPUTPROCESSOR_H */

