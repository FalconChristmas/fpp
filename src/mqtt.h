#pragma once
/*
 *   Mosquitto Client for Falcon Player (FPP)
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

#include <functional>
#include <map>
#include <mutex>
#include <pthread.h>
#include <string>

#include "Warnings.h"

#include "mosquitto.h"

void* RunMqttPublishThread(void* data);

class MosquittoClient : public WarningListener {
public:
    MosquittoClient(const std::string& host, const int port, const std::string& topicPrefix);
    ~MosquittoClient();

    int Init(const std::string& username, const std::string& password, const std::string& ca_file);

    void PrepareForShutdown();

    int PublishRaw(const std::string& topic, const std::string& msg, const bool retain = false, const int qos = 1);
    int Publish(const std::string& subTopic, const std::string& msg, const bool retain = false, const int qos = 1);
    int Publish(const std::string& subTopic, const int valueconst, bool retain = false, const int qos = 1);

    void LogCallback(void* userdata, int level, const char* str);
    void MessageCallback(void* obj, const struct mosquitto_message* message);

    void AddCallback(const std::string& topic, std::function<void(const std::string& topic, const std::string& payload)>& callback);
    virtual void handleWarnings(std::list<std::string>& warnings);

    void RemoveCallback(const std::string& topic);
    void HandleDisconnect();
    void HandleConnect();
    bool IsConnected();

    void PublishStatus();
    void SetReady();

    void CacheSetMessage(std::string& topic, std::string& message);
    std::string CacheGetMessage(std::string& topic);
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

    std::map<std::string, std::function<void(const std::string& topic, const std::string& payload)>> callbacks;

    std::mutex messageCacheLock;
    std::map<std::string, std::string> messageCache;
};

extern MosquittoClient* mqtt;
