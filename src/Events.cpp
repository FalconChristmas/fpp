/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <algorithm>
#include <climits>
#include <ctime>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>

#include "OutputMonitor.h"
#include "Player.h"
#include "Warnings.h"
#include "common.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "Events.h"

static std::list<EventHandler*> EVENT_HANDLERS;
static std::map<std::string, std::function<void(const std::string& topic, const std::string& payload)>> EVENT_CALLBACKS;
static std::thread* EVENT_PUBLISH_THREAD = nullptr;
static volatile bool EVENT_PUBLISH_THREAD_RUN = true;
static std::vector<EventNotifier*> eventNotifiers;

class PublishPlaylistStatus : public EventNotifier {
public:
    PublishPlaylistStatus(int freq) :
        EventNotifier(freq) {}
    ~PublishPlaylistStatus() {}

    void notify() {
        Json::Value json = Player::INSTANCE.GetMqttStatusJSON();
        std::stringstream buffer;
        buffer << json << std::endl;
        LogDebug(VB_CONTROL, "Notify PublishPlaylist\n");
        Events::Publish("playlist_details", buffer.str());
    }
};

extern void GetCurrentFPPDStatus(Json::Value& result);

class PublishFPPDStatus : public EventNotifier {
public:
    PublishFPPDStatus(int freq) :
        EventNotifier(freq) {}
    ~PublishFPPDStatus() {}

    void notify() {
        Json::Value json;
        GetCurrentFPPDStatus(json);

        std::stringstream buffer;
        buffer << json << std::endl;
        LogDebug(VB_CONTROL, "Notify FPPDStatus\n");
        Events::Publish("fppd_status", buffer.str());
    }
};

class PublishPortStatus : public EventNotifier {
public:
    PublishPortStatus(int freq) :
        EventNotifier(freq) {}
    ~PublishPortStatus() {}

    void notify() {
        Json::Value json = Json::arrayValue;
        OutputMonitor::INSTANCE.GetCurrentPortStatusJson(json);
        std::stringstream buffer;
        buffer << json << std::endl;
        LogDebug(VB_CONTROL, "Notify FPPDStatus\n");
        Events::Publish("port_status", buffer.str());
    }
};

class EventWarningListener : public WarningListener {
public:
    EventWarningListener() {}
    virtual ~EventWarningListener() {}

    virtual void handleWarnings(const std::list<FPPWarning>& warnings) {
        if (Events::HasEventHandlers()) {
            Json::Value rc = Json::Value(Json::arrayValue);
            for (auto &w: warnings) {
                rc.append(w);
            }
            std::string msg = SaveJsonToString(rc);
            LogDebug(VB_CONTROL, "Sending warning message: %s\n", msg.c_str());
            Events::Publish("warnings", msg);
        }
    }
};
static EventWarningListener* EVENT_WARNING_LISTENER = nullptr;

EventHandler::EventHandler() {}
EventHandler::~EventHandler() {}

void Events::Ready() {
    if (EVENT_WARNING_LISTENER == nullptr) {
        EVENT_WARNING_LISTENER = new EventWarningListener();
        WarningHolder::AddWarningListener(EVENT_WARNING_LISTENER);
    }

    Events::Publish(MQTT_READY_TOPIC_NAME, 1);
    Events::Publish("version", getFPPVersion());
    Events::Publish("branch", getFPPBranch());

    int playlistFrequency = getSettingInt("MQTTFrequency");
    int statusFrequency = getSettingInt("MQTTStatusFrequency");
    int portFrequency = getSettingInt("MQTTPortStatusFrequency");

    if (playlistFrequency > 0) {
        eventNotifiers.push_back(new PublishPlaylistStatus(playlistFrequency));
    }

    if (statusFrequency > 0) {
        eventNotifiers.push_back(new PublishFPPDStatus(statusFrequency));
    }

    if (portFrequency > 0) {
        eventNotifiers.push_back(new PublishPortStatus(portFrequency));
    }

    if (EVENT_PUBLISH_THREAD == nullptr && eventNotifiers.size() > 0) {
        EVENT_PUBLISH_THREAD = new std::thread();
        // create  background Publish Thread
        EVENT_PUBLISH_THREAD = new std::thread(Events::RunPublishThread);
    }
}

void Events::AddEventHandler(EventHandler* handler) {
    for (auto& c : EVENT_CALLBACKS) {
        std::string topic = c.first;
        handler->RegisterCallback(topic);
    }
    EVENT_HANDLERS.push_back(handler);
}
void Events::RemoveEventHandler(EventHandler* handler) {
    for (auto& c : EVENT_CALLBACKS) {
        handler->RemoveCallback(c.first);
    }
    EVENT_HANDLERS.remove(handler);
}

bool Events::HasEventHandlers() {
    return !EVENT_HANDLERS.empty();
}

bool Events::Publish(const std::string& topic, const std::string& data) {
    bool ret = true;
    for (auto handler : EVENT_HANDLERS) {
        ret &= handler->Publish(topic, data);
    }
    return ret;
}

bool Events::Publish(const std::string& topic, const int valueconst) {
    bool ret = true;
    for (auto handler : EVENT_HANDLERS) {
        ret &= handler->Publish(topic, valueconst);
    }
    return ret;
}

void Events::AddCallback(const std::string& topic, std::function<void(const std::string& topic, const std::string& payload)>& callback) {
    EVENT_CALLBACKS[topic] = callback;
    for (auto handler : EVENT_HANDLERS) {
        handler->RegisterCallback(topic);
    }
}
void Events::RemoveCallback(const std::string& topic) {
    for (auto handler : EVENT_HANDLERS) {
        handler->RemoveCallback(topic);
    }
    EVENT_CALLBACKS.erase(topic);
}
void Events::InvokeCallback(const std::string& topic, const std::string& topic_in, const std::string& payload) {
    EVENT_CALLBACKS[topic](topic_in, payload);
}

void Events::PrepareForShutdown() {
    if (EVENT_PUBLISH_THREAD) {
        EVENT_PUBLISH_THREAD_RUN = false;
    }
    if (EVENT_WARNING_LISTENER != nullptr) {
        WarningHolder::RemoveWarningListener(EVENT_WARNING_LISTENER);
        delete EVENT_WARNING_LISTENER;
        EVENT_WARNING_LISTENER = nullptr;
    }

    Publish(MQTT_READY_TOPIC_NAME, 0);
    for (auto handler : EVENT_HANDLERS) {
        handler->PrepareForShutdown();
    }
}

void Events::RunPublishThread() {
    SetThreadName("FPP-Events");
    sleep(3); // Give everything time to start up

    LogInfo(VB_CONTROL, "Starting Publish Thread\n");
    if (eventNotifiers.size() == 0) {
        // kill thread
        LogInfo(VB_CONTROL, "Stopping Publish Thread as frequency is zero.\n");
        return;
    }

    // Loop for ever  We are running both publishes in one thread
    // just to minimize the number of threads
    time_t lastStatus = std::time(0);
    time_t lastPlaylist = std::time(0);
    while (EVENT_PUBLISH_THREAD_RUN) {
        time_t now = std::time(0);
        int sleep_dur = INT_MAX;

        for (auto it = eventNotifiers.begin(); it != eventNotifiers.end(); it++) {
            if ((*it)->next_time <= now) {
                (*it)->notify();
                (*it)->next_time = (now + (*it)->frequency);
            }
            sleep_dur = std::min(sleep_dur, (int)((*it)->next_time - now));
        }
        sleep(sleep_dur);
    }
}
