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
    char id[6];
    sprintf(id, "%02d_%02d", majorID, minorID);

    std::string filename = getEventDirectory();
    filename = filename + "/" + id + ".fevt";
    return filename;
}


FPPEvent::FPPEvent(uint8_t major, uint8_t minor) : minorID(1), majorID(1) {
    char id[6];
    sprintf(id, "%02d_%02d", major, minor);
    Load(id);
}

FPPEvent::FPPEvent(const std::string &id) {
    Load(id);
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
        Json::Value val = JSONStringToObject(contents);
        name = val["name"].asString();
        minorID = val["minorId"].asInt();
        majorID = val["majorId"].asInt();
        command = val["command"].asString();
        for (int x = 0; x < val["args"].size(); x++) {
            args.push_back(val["args"][x].asString());
        }
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
                minorID = std::atoi(val.c_str());
            } else if (elems[0] == "majorID") {
                majorID = std::atoi(val.c_str());
            } else if (elems[0] == "name") {
                name = val;
            } else if (elems[0] == "effect" && val != "") {
                effectArgs.insert(args.begin(), "false");
                effectArgs.insert(args.begin(), val);
            } else if (elems[0] == "startChannel" && val != "") {
                effectArgs.push_back(val);
            } else if (elems[0] == "script") {
                scriptArgs.insert(args.begin(), val);
            } else if (elems[0] == "scriptArgs") {
                scriptArgs.push_back(val);
            }
        }
        if (!scriptArgs.empty()) {
            command = "Run Script";
            args = scriptArgs;
        } else {
            command = "Play Effect";
            args = effectArgs;
        }
        save();
    }
    
    if (command == "Run Script") {
        //need to add the script environment
        while (args.size() < 3) {
            args.push_back("");
        }
        std::string env = "FPP_EVENT_MAJOR_ID=";
        env += std::to_string(majorID);
        env += ",FPP_EVENT_MINOR_ID=";
        env += std::to_string(minorID);
        env += ",FPP_EVENT_NAME=";
        env += name;
        env += ",FPP_EVENT_SCRIPT=";
        env += args[0];
        args[2] = env;
    }
}
Json::Value FPPEvent::toJsonValue() {
    Json::Value ret;
    ret["majorId"] = majorID;
    ret["minorId"] = minorID;
    ret["name"] = name;
    ret["command"] = command;
    for (auto &a : args) {
        ret["args"].append(a);
    }
    return ret;
}
void FPPEvent::save() {
    Json::Value value = toJsonValue();
    std::string filename = getEventFileName();

    std::ofstream ostream(filename);
    Json::StyledWriter writer;
    ostream << writer.write(value);
    ostream.close();
}



/*
 * Load an event file into a FPPevent
 */
FPPEvent* LoadEvent(const char *id)
{
	FPPEvent *event = new FPPEvent(id);
    
    if (event->command == "") {
        delete event;
        return nullptr;
    }

	LogDebug(VB_EVENT, "Event Loaded:\n");
	if (event->name != "")
		LogDebug(VB_EVENT, "Event Name  : %s\n", event->name.c_str());
	else
		LogDebug(VB_EVENT, "Event Name  : ERROR, no name defined in event file\n");
	LogDebug(VB_EVENT, "Event ID    : %d/%d\n", event->majorID, event->minorID);

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

    CommandManager::INSTANCE.run(event->command, event->args);

    delete event;
	return 1;
}

void UpgradeEvents() {
    char id[6];
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
