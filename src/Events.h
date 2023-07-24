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

class EventHandler {
public:
    EventHandler();
    virtual ~EventHandler();

    virtual bool Publish(const std::string& topic, const std::string& data) = 0;
    virtual bool Publish(const std::string& subTopic, const int valueconst) = 0;

    virtual void RegisterCallback(const std::string& topic) = 0;
    virtual void RemoveCallback(const std::string& topic) = 0;

    virtual void PrepareForShutdown() = 0;
};

class Events {
public:
    static void Ready();

    static bool HasEventHandlers();
    static void AddEventHandler(EventHandler* handler);
    static void RemoveEventHandler(EventHandler* handler);

    static bool Publish(const std::string& topic, const std::string& data);
    static bool Publish(const std::string& subTopic, const int valueconst);

    static void AddCallback(const std::string& topic, std::function<void(const std::string& topic, const std::string& payload)>& callback);
    static void RemoveCallback(const std::string& topic);

    static void PrepareForShutdown();

    static void InvokeCallback(const std::string& topic, const std::string& topic_in, const std::string& payload);

private:
    static void RunPublishThread();
    static void PublishPlaylistStatus();
    static void PublishFPPDStatus();
};