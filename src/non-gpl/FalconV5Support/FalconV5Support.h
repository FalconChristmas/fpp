#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include <functional>
#include <list>
#include <map>

class PixelString;
class FalconV5Listener;

class FalconV5Support {
public:
    FalconV5Support();
    ~FalconV5Support();

    class ReceiverChain {
    public:
        ReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int grp = 0) :
            group(grp) {
            strings[0] = p1;
            strings[1] = p2;
            strings[2] = p3;
            strings[3] = p4;
        }
        bool generateConfigPacket(uint8_t* packet) const;

        const std::array<const PixelString*, 4>& getPixelStrings() const { return strings; };

    private:
        std::array<const PixelString*, 4> strings;
        uint32_t group;
    };

    ReceiverChain* addReceiverChain(PixelString* p1, PixelString* p2, PixelString* p3, PixelString* p4, int group);
    const std::list<const ReceiverChain*>& getReceiverChains() const { return receiverChains; };

    void addListeners(Json::Value& config);

    void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks);

private:
    std::list<const ReceiverChain*> receiverChains;
    std::list<FalconV5Listener*> listeners;
};