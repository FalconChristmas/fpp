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
#include <map>
#include <mutex>
#include <pthread.h>
#include <string>

#include "Warnings.h"
#include "Events.h"

#include "mosquitto.h"

class MosquittoClient : public EventHandler {
public:
    MosquittoClient(const std::string& host, const int port, const std::string& topicPrefix);
    ~MosquittoClient();

    int Init(const std::string& username, const std::string& password, const std::string& ca_file);

    virtual bool Publish(const std::string &topic, const std::string &data) override;
    virtual bool Publish(const std::string& subTopic, const int valueconst) override;
    virtual void RegisterCallback(const std::string& topic) override;
    virtual void RemoveCallback(const std::string& topic) override;


    virtual void PrepareForShutdown() override;

    int PublishRaw(const std::string& topic, const std::string& msg, const bool retain = false, const int qos = 1);
    int Publish(const std::string& subTopic, const std::string& msg, const bool retain, const int qos = 1);
    int Publish(const std::string& subTopic, const int valueconst, bool retain, const int qos = 1);



    void LogCallback(void* userdata, int level, const char* str);
    void MessageCallback(void* obj, const struct mosquitto_message* message);

    void HandleDisconnect();
    void HandleConnect();
    bool IsConnected();

    void SetReady();

    void CacheSetMessage(std::string& topic, std::string& message);
    bool CacheCheckMessage(std::string& topic, std::string& message);

    std::string GetBaseTopic() { return m_baseTopic; }

private:
    bool m_canProcessMessages;
    bool m_isConnected;
    std::string m_host;
    int m_port;
    int m_keepalive;

    std::string m_topicPrefix;
    std::string m_baseTopic;
    std::string m_subBaseTopic;

    struct mosquitto* m_mosq;
    pthread_mutex_t m_mosqLock;
    pthread_t m_mqtt_publish_t;

    std::list<std::string> callbackTopics;

    std::mutex messageCacheLock;
    std::map<std::string, std::string> messageCache;
};

extern MosquittoClient* mqtt;
