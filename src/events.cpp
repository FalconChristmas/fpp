/*
 *   Events handler for Falcon Player (FPP)
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

#include <fstream>
#include <sstream>

#include "events.h"
#include "settings.h"
#include "log.h"
#include "common.h"

#include "commands/Commands.h"
#include "Plugins.h"
#include "MultiSync.h"

std::string FPPEvent::getEventFileName(const std::string &id) {
    std::string filename = getEventDirectory();
    filename = filename + "/" + id + ".fevt";
    return filename;
}
std::string FPPEvent::getEventFileName() {
    char id[6] = {0};
    
    int minorID = event["minorId"].asInt();
    int majorID = event["majorId"].asInt();

    sprintf(id, "%02d_%02d", majorID, minorID);

    std::string filename = getEventDirectory();
    filename = filename + "/" + id + ".fevt";
    return filename;
}


FPPEvent::FPPEvent(uint8_t major, uint8_t minor) {
    char id[6];
    sprintf(id, "%02d_%02d", major, minor);
    Load(id);
    event["minorId"] = minor;
    event["majorId"] = major;
}

FPPEvent::FPPEvent(const std::string &id) {
    Load(id);
}
int FPPEvent::getMajorId() {
    return event["majorId"].asInt();
}
int FPPEvent::getMinorId() {
    return event["minorId"].asInt();
}
std::string FPPEvent::getName() {
    return event["name"].asString();
}


inline void insertAtStart(std::vector<std::string> &a, const std::string &s) {
    if (a.empty()) {
        a.push_back(s);
    } else {
        a.insert(a.begin(), s);
    }
}

void FPPEvent::Load(const std::string &id) {
    std::string filename = getEventFileName(id);
    if (!FileExists(filename)) {
        return;
    }
    std::string contents = GetFileContents(filename);

    if (contents.empty()) {
        LogErr(VB_EVENT, "Unable to open Event file %s\n", filename);
        return;
    }
    
    if (contents[0] == '{') {
        //new JSON format
        event = LoadJsonFromString(contents);
    } else {
        //old line format
        std::stringstream ss(contents);
        std::string to;
        
        std::vector<std::string> effectArgs;
        std::vector<std::string> scriptArgs;
        while (std::getline(ss, to, '\n')) {
            std::vector<std::string> elems = splitWithQuotes(to, '=');
            while (elems.size() < 2) {
                elems.push_back("");
            }
            std::string val = elems[1];
            TrimWhiteSpace(elems[0]);
            TrimWhiteSpace(val);
            
            if (val.size() > 1 && val[0] == '\'') {
                val = val.substr(1, val.size() - 2);
            }
            
            if (elems[0] == "minorID") {
                event["minorId"] = std::atoi(val.c_str());
            } else if (elems[0] == "majorID") {
                event["majorId"] = std::atoi(val.c_str());
            } else if (elems[0] == "name") {
                event["name"] = val;
            } else if (elems[0] == "effect" && val != "") {
                insertAtStart(effectArgs, "false");
                insertAtStart(effectArgs, val);
            } else if (elems[0] == "startChannel" && val != "") {
                effectArgs.push_back(val);
            } else if (elems[0] == "script") {
                insertAtStart(scriptArgs, val);
            } else if (elems[0] == "scriptArgs") {
                scriptArgs.push_back(val);
            }
        }
        if (!scriptArgs.empty()) {
            event["command"] = "Run Script";
            for (auto &a : scriptArgs) {
                event["args"].append(a);
            }
        } else {
            event["command"] = "Play Effect";
            for (auto &a : effectArgs) {
                event["args"].append(a);
            }
        }
        save();
    }
    
    if (event["command"].asString() == "Run Script") {
        //need to add the script environment
        while (event["args"].size() < 3) {
            event["args"].append("");
        }
        std::string env = "FPP_EVENT_MAJOR_ID=";
        env += std::to_string(event["majorId"].asInt());
        env += ",FPP_EVENT_MINOR_ID=";
        env += std::to_string(event["minorId"].asInt());
        env += ",FPP_EVENT_NAME=";
        env += event["name"].asString();
        env += ",FPP_EVENT_SCRIPT=";
        env += event["args"][0].asString();
        event["args"][2] = env;
    }
}
void FPPEvent::save() {
    SaveJsonToFile(toJsonValue(), getEventFileName(), "\t");
}



/*
 * Load an event file into a FPPevent
 */
FPPEvent* LoadEvent(const char *id)
{
	FPPEvent *event = new FPPEvent(id);
    
    if (!event->toJsonValue().isMember("command")
        || event->toJsonValue()["command"].asString() == "") {
        delete event;
        return nullptr;
    }

	LogDebug(VB_EVENT, "Event Loaded:\n");
	if (event->getName() != "")
		LogDebug(VB_EVENT, "Event Name  : %s\n", event->getName().c_str());
	else
		LogDebug(VB_EVENT, "Event Name  : ERROR, no name defined in event file\n");
	LogDebug(VB_EVENT, "Event ID    : %d/%d\n", event->getMajorId(), event->getMinorId());

	return event;
}

/*
 * Trigger an event by major/minor number
 */
int TriggerEvent(const char major, const char minor)
{
	LogDebug(VB_EVENT, "TriggerEvent(%d, %d)\n", (unsigned char)major, (unsigned char)minor);

	if ((major > MAX_EVENT_MAJOR) || (major < 1) || (minor > MAX_EVENT_MINOR) || (minor < 1))
		return 0;

	char id[6];
	sprintf(id, "%02d_%02d", major, minor);
	return TriggerEventByID(id);
}

/*
 * Trigger an event
 */
int TriggerEventByID(const char *id)
{
	LogDebug(VB_EVENT, "TriggerEventByID(%s)\n", id);
    PluginManager::INSTANCE.eventCallback(id, "sequence");

	if (getFPPmode() == MASTER_MODE)
		multiSync->SendEventPacket(id);

	FPPEvent *event = LoadEvent(id);

	if (!event) {
		if (getFPPmode() != REMOTE_MODE)
			LogWarn(VB_EVENT, "Unable to load event %s\n", id);
		return 0;
	}

    CommandManager::INSTANCE.run(event->toJsonValue());

    delete event;
	return 1;
}

void UpgradeEvents() {
    char id[6];
    memset(id, 0, sizeof(id));
    for (int major = 1; major <= MAX_EVENT_MAJOR; major++) {
        for (int minor = 1; minor <= MAX_EVENT_MAJOR; minor++) {
            sprintf(id, "%02d_%02d", major, minor);
            std::string filename = FPPEvent::getEventFileName(id);
            if (FileExists(filename)) {
                std::string contents = GetFileContents(filename);
                if (contents.size() > 0 && contents[0] != '{') {
                    FPPEvent *e = new FPPEvent(id);
                    delete e;
                }
            }
        }
    }
}
