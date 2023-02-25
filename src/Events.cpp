/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2023 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "Events.h"
#include "Player.h"
#include <climits>

static std::list<EventHandler *> EVENT_HANDLERS;
static std::map<std::string, std::function<void(const std::string& topic, const std::string& payload)>> EVENT_CALLBACKS; 
static std::thread *EVENT_PUBLISH_THREAD = nullptr;
static volatile bool EVENT_PUBLISH_THREAD_RUN = true;

class EventWarningListener : public WarningListener {
public:
    EventWarningListener() {}
    virtual ~EventWarningListener() {}

    virtual void handleWarnings(std::list<std::string>& warnings) {
        if (Events::HasEventHandlers()) {
            Json::Value rc = Json::Value(Json::arrayValue);
            for (std::list<std::string>::iterator it = warnings.begin(); it != warnings.end(); ++it) {
                rc.append(*it);
            }

            std::string msg = SaveJsonToString(rc);
            LogDebug(VB_CONTROL, "Sending warning message: %s\n", msg.c_str());
            Events::Publish("warnings", msg);
        }
    }
};
static EventWarningListener *EVENT_WARNING_LISTENER = nullptr;


EventHandler::EventHandler() {}
EventHandler::~EventHandler() {}

void Events::Ready() {
    if (EVENT_WARNING_LISTENER == nullptr) {
        EVENT_WARNING_LISTENER = new EventWarningListener();
        WarningHolder::AddWarningListener(EVENT_WARNING_LISTENER);
    }

    Events::Publish("ready", 1);
    Events::Publish("version", getFPPVersion());
    Events::Publish("branch", getFPPBranch());

    int playlistFrequency = getSettingInt("MQTTFrequency");
    int statusFrequency = getSettingInt("MQTTStatusFrequency");
    if (EVENT_PUBLISH_THREAD == nullptr && (playlistFrequency > 0) || (statusFrequency > 0)) {
        EVENT_PUBLISH_THREAD = new std::thread();
        // create  background Publish Thread
        EVENT_PUBLISH_THREAD = new std::thread(Events::RunPublishThread);
    }
}


void Events::AddEventHandler(EventHandler *handler) {
    for (auto &c : EVENT_CALLBACKS) {
        std::string topic = c.first;
        handler->RegisterCallback(topic);
    }
    EVENT_HANDLERS.push_back(handler);
}
void Events::RemoveEventHandler(EventHandler *handler) {
    for (auto &c : EVENT_CALLBACKS) {
        handler->RemoveCallback(c.first);
    }
    EVENT_HANDLERS.remove(handler);
}

bool Events::HasEventHandlers() {
    return !EVENT_HANDLERS.empty();
}

bool Events::Publish(const std::string &topic, const std::string &data) {
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
void Events::InvokeCallback(const std::string &topic, const std::string &topic_in, const std::string &payload) {
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

    Publish("ready", 0);
    for (auto handler : EVENT_HANDLERS) {
        handler->PrepareForShutdown();
    }
}

void Events::RunPublishThread() {
    sleep(3); // Give everything time to start up

    int playlistFrequency = getSettingInt("MQTTFrequency");
    int statusFrequency = getSettingInt("MQTTStatusFrequency");
    if (playlistFrequency < 0) {
        playlistFrequency = 0;
    }
    if (statusFrequency < 0) {
        statusFrequency = 0;
    }

    LogInfo(VB_CONTROL, "Starting Publish Thread with Playlist Frequency: %d and Status Frequency %d\n", playlistFrequency, statusFrequency);
    if ((playlistFrequency == 0) && (statusFrequency == 0)) {
        // kill thread
        LogInfo(VB_CONTROL, "Stopping Publish Thread as frequency is zero.\nc");
        return;
    }

    // Loop for ever  We are running both publishes in one thread
    // just to minimize the number of threads
    time_t lastStatus = std::time(0);
    time_t lastPlaylist = std::time(0);
    while (EVENT_PUBLISH_THREAD_RUN) {
        time_t now = std::time(0);
        int statusDiff = statusFrequency == 0 ? INT_MIN : (int)(now - lastStatus);
        int playlistDiff = playlistFrequency == 0 ? INT_MIN : (int)(now - lastPlaylist);
        if (playlistDiff >= playlistFrequency) {
            Events::PublishPlaylistStatus();
            lastPlaylist = now;
            playlistDiff = 0;
        }
        if (statusDiff >= statusFrequency) {
            Events::PublishFPPDStatus();
            lastStatus = now;
            statusDiff = 0;
        }
        int playlistSleep = playlistFrequency == 0 ? INT_MAX : (playlistFrequency - playlistDiff);
        int statusSleep = statusFrequency == 0 ? INT_MAX : (statusFrequency - statusDiff);

        sleep(std::min(playlistSleep, statusSleep));
    }
}
void Events::PublishPlaylistStatus() {
    Json::Value json = Player::INSTANCE.GetMqttStatusJSON();

    std::stringstream buffer;
    buffer << json << std::endl;
    Publish("playlist_details", buffer.str());
}
extern void GetCurrentFPPDStatus(Json::Value& result);
void Events::PublishFPPDStatus() {
    Json::Value json;
    GetCurrentFPPDStatus(json);

    std::stringstream buffer;
    buffer << json << std::endl;
    Publish("fppd_status", buffer.str());
}