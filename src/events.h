/*
 *   Events handler for Falcon Player (FPP)
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

// events.h
#ifndef EVENTS_H_
#define EVENTS_H_

#include <string>
#include <vector>
#include <jsoncpp/json/json.h>

#define MAX_EVENT_MAJOR 25
#define MAX_EVENT_MINOR 25


class FPPEvent {
public:
    FPPEvent(const std::string &id);
    FPPEvent(uint8_t major, uint8_t minor);
    const Json::Value &toJsonValue() const { return event; };
    void save();
        
    static std::string getEventFileName(const std::string &id);
    std::string getEventFileName();
    
    int getMajorId();
    int getMinorId();
    std::string getName();
private:
    void Load(const std::string &id);
    Json::Value event;
};

void UpgradeEvents();
int TriggerEvent(const char major, const char minor);
int TriggerEventByID(const char *ID);
FPPEvent* LoadEvent(const char *id);

#endif
