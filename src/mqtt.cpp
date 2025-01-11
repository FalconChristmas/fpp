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

#include "fpp-pch.h"

#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mosquitto.h>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "Events.h"
#include "Timers.h"
#include "Warnings.h"
#include "log.h"
#include "mqtt.h"
#include "settings.h"
#include "commands/Commands.h"

#define FALCON_TOPIC "falcon/player"

MosquittoClient* mqtt = NULL;

class MQTTCommand : public Command {
public:
    MQTTCommand() :
        Command("MQTT") {
        args.push_back(CommandArg("topic", "string", "Topic"));
        args.push_back(CommandArg("message", "string", "Message"));
        args.push_back(CommandArg("retain", "bool", "Retain").setDefaultValue("false"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            return std::make_unique<Command::ErrorResult>("MQTT Requires two arguments");
        }
        bool retain = false;
        if ((args.size() >= 3) &&
            ((args[2] == "true") ||
             (args[2] == "1"))) {
            retain = true;
        }

        mqtt->PublishRaw(args[0], args[1], retain);
        return std::make_unique<Command::Result>("OK");
    }
};

void mosq_disc_callback(struct mosquitto* mosq, void* userdata, int level) {
    if (userdata) {
        ((MosquittoClient*)userdata)->HandleDisconnect();
    }
    /*
     * Don't call reconnect here. Per documentation, loop_start() will handle
     * reconnect automatically.
     */
    // mosquitto_reconnect(mosq);
}

void mosq_connect_callback(struct mosquitto* mosq, void* userdata, int level) {
    if (userdata) {
        ((MosquittoClient*)userdata)->HandleConnect();
    }
}

void mosq_log_callback(struct mosquitto* mosq, void* userdata, int level, const char* str) {
    mqtt->LogCallback(userdata, level, str);
}

void mosq_msg_callback(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* message) {
    mqtt->MessageCallback(userdata, message);
}

/*
 *
 */
MosquittoClient::MosquittoClient(const std::string& host, const int port,
                                 const std::string& topicPrefix) :
    m_port(port),
    m_keepalive(60),
    m_mosq(NULL),
    m_host(host),
    m_canProcessMessages(false),
    m_isConnected(false),
    m_topicPrefix(topicPrefix) {
    LogDebug(VB_CONTROL, "MosquittoClient::MosquittoClient('%s', %d, '%s')\n",
             host.c_str(), port, topicPrefix.c_str());

    WarningHolder::AddWarning(4, "MQTT Disconnected");
    if (m_topicPrefix.size()) {
        m_topicPrefix += "/";
    }

    m_baseTopic = m_topicPrefix;
    m_baseTopic += FALCON_TOPIC;
    m_baseTopic += "/";

    std::string hostname = getSetting("HostName");
    if (hostname == "") {
        hostname = "FPP";
    }
    m_baseTopic += hostname;

    pthread_mutex_init(&m_mosqLock, NULL);

} // End of MosquittoClient()

/*
 *
 */
MosquittoClient::~MosquittoClient() {
    mosquitto_loop_stop(m_mosq, true);

    sleep(1);

    mosquitto_destroy(m_mosq);
    mqtt = NULL;

    mosquitto_lib_cleanup();

    pthread_mutex_destroy(&m_mosqLock);
}

void MosquittoClient::PrepareForShutdown() {
    while (!callbackTopics.empty()) {
        const std::string& n = callbackTopics.front();
        RemoveCallback(n);
    }
    if (m_canProcessMessages && m_isConnected) {
        std::vector<std::string> subscribe_topics;
        subscribe_topics.push_back(m_baseTopic + "/set/#");

        for (auto& subscribe : subscribe_topics) {
            mosquitto_unsubscribe(m_mosq, NULL, subscribe.c_str());
        }
    }
    m_canProcessMessages = false;
}

/*
 *
 */
int MosquittoClient::Init(const std::string& username, const std::string& password, const std::string& ca_file) {
    mosquitto_lib_init();
    // Use supplied Client Id (If one)
    std::string clientId = getSetting("MQTTClientId");
    if (clientId == "") {
        // If not, genereate one with random digits on end
        clientId = getSetting("HostName");
        if (clientId == "") {
            clientId = "FPP";
        }
        clientId.append("_");
        for (int i = 0; i < 4; i++) {
            int digit = rand() % 10;
            char digitc = '0' + digit;
            clientId.append(1, digitc);
        }
    }
    LogInfo(VB_CONTROL, "Using MQTT client id of %s \n", clientId.c_str());
    m_mosq = mosquitto_new(clientId.c_str(), true, NULL);
    if (!m_mosq) {
        LogErr(VB_CONTROL, "Error, unable to create new Mosquitto instance.\n");
        return 0;
    }
    mosquitto_user_data_set(m_mosq, this);
    mosquitto_log_callback_set(m_mosq, mosq_log_callback);
    mosquitto_disconnect_callback_set(m_mosq, mosq_disc_callback);
    mosquitto_connect_callback_set(m_mosq, mosq_connect_callback);

    if (username != "") {
        mosquitto_username_pw_set(m_mosq, username.c_str(), password.c_str());
    }

    if (ca_file != "") {
        LogInfo(VB_CONTROL, "Using CA File: %s for MQTT\n", ca_file.c_str());
        int rc = mosquitto_tls_set(m_mosq, ca_file.c_str(), NULL, NULL, NULL, NULL);
        if (rc) {
            LogErr(VB_CONTROL, "Error, unable to set MQTT_Ca_file. RC=  %d\n", rc);
            return 0;
        }
    } else {
        LogInfo(VB_CONTROL, "No CA File specified for MQTT\n");
    }

    LogDebug(VB_CONTROL, "About to call MQTT Connect (%s, %d, %d)\n", m_host.c_str(), m_port, m_keepalive);

    // Tell MQTT broker to make MQTT "ready" topic go to zero if crash or network loss
    static std::string last_ready_message = "0";
    static std::string last_ready_topic = m_baseTopic + "/" + MQTT_READY_TOPIC_NAME;
    int rc = mosquitto_will_set(m_mosq, last_ready_topic.c_str(), last_ready_message.size(), last_ready_message.c_str(), 1, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        LogErr(VB_CONTROL, "MQTT: Unable to set last will for  %s. Error code: %d\n", MQTT_READY_TOPIC_NAME, rc);
    }

    rc = mosquitto_connect_async(m_mosq, m_host.c_str(), m_port, m_keepalive);

    if (rc) {
        LogErr(VB_CONTROL, "Error, unable to connect to Mosquitto Broker at %s: %d\n",
               m_host.c_str(), rc);
        LogErr(VB_CONTROL, "MQTT Error: %s\n", strerror(rc));
        return 0;
    }

    int loop = mosquitto_loop_start(m_mosq);
    if (loop != MOSQ_ERR_SUCCESS) {
        LogErr(VB_CONTROL, "Error, unable to start Mosquitto loop: %d\n", loop);
        return 0;
    }

    CommandManager::INSTANCE.addCommand(new MQTTCommand());

    return 1;
}

/*
 *
 */
int MosquittoClient::PublishRaw(const std::string& topic, const std::string& msg, const bool retain, const int qos) {
    LogDebug(VB_CONTROL, "Publishing message '%s' on topic '%s'\n", msg.c_str(), topic.c_str());

    pthread_mutex_lock(&m_mosqLock);

    int result = mosquitto_publish(m_mosq, NULL, topic.c_str(), msg.size(), msg.c_str(), qos, retain);

    pthread_mutex_unlock(&m_mosqLock);

    if (result != 0) {
        LogErr(VB_CONTROL, "Error running mosquitto_publish: %d\n", result);
        return 0;
    }

    return 1;
}

bool MosquittoClient::Publish(const std::string& topic, const std::string& data) {
    if (topic == "version" || topic == "branch" || topic == "warnings") {
        return Publish(topic, data, true, 1);
    }
    return Publish(topic, data, false, 1);
}
bool MosquittoClient::Publish(const std::string& topic, const int value) {
    if (topic == MQTT_READY_TOPIC_NAME) {
        if (value) {
            SetReady();
        }
        return Publish(topic, value, true, 1);
    }
    return Publish(topic, value, false, 1);
}

/*
 *
 */
int MosquittoClient::Publish(const std::string& subTopic, const std::string& msg, const bool retain, const int qos) {
    std::string topic = m_baseTopic + "/" + subTopic;

    return PublishRaw(topic, msg, qos, retain);
}

/*
 *
 */
int MosquittoClient::Publish(const std::string& subTopic, const int value, const bool retain, const int qos) {
    std::string topic = m_baseTopic + "/" + subTopic;
    std::string msg = std::to_string(value);

    return PublishRaw(topic, msg, qos, retain);
}

/*
 *
 */
void MosquittoClient::LogCallback(void* userdata, int level, const char* str) {
    switch (level) {
        // FIXME, comment out the first 3 of these or tie to our own debug level
        //		case MOSQ_LOG_DEBUG:
        //		case MOSQ_LOG_INFO:
        //		case MOSQ_LOG_NOTICE:
    case MOSQ_LOG_WARNING:
    case MOSQ_LOG_ERR:
        LogDebug(VB_CONTROL, "Mosquitto Log: %d:%s\n", level, str);
    }
}

bool MosquittoClient::IsConnected() {
    return m_isConnected;
}

void MosquittoClient::RegisterCallback(const std::string& topic) {
    LogDebug(VB_CONTROL, "MQTT: In AddCallback with %s\n", topic.c_str());
    callbackTopics.push_back(topic);
    if (m_canProcessMessages && m_isConnected) {
        if (topic.rfind("/set/", 0) != 0) {
            // we are registered on all "/set/" already, no need to re-register
            std::string tp = m_baseTopic + topic;
            LogDebug(VB_CONTROL, "MQTT: Preparing to Subscribe to %s\n", tp.c_str());
            int rc = mosquitto_subscribe(m_mosq, NULL, tp.c_str(), 0);
            if (rc != MOSQ_ERR_SUCCESS) {
                LogErr(VB_CONTROL, "Error, unable to subscribe to %s: %d\n", tp.c_str(), rc);
            }
        }
    }
}
void MosquittoClient::RemoveCallback(const std::string& topic) {
    if (m_canProcessMessages && m_isConnected) {
        if (topic.rfind("/set/", 0) != 0) {
            // we are registered on all "/set/" already, no need to un-register
            std::string tp = m_baseTopic + topic;
            LogDebug(VB_CONTROL, "MQTT: Preparing to Unsubscribe to %s\n", tp.c_str());
            int rc = mosquitto_unsubscribe(m_mosq, NULL, tp.c_str());
            if (rc != MOSQ_ERR_SUCCESS) {
                LogErr(VB_CONTROL, "Error, unable to Unsubscribe to %s: %d\n", tp.c_str(), rc);
            }
        }
    }
    callbackTopics.remove(topic);
}
void MosquittoClient::SetReady() {
    LogInfo(VB_CONTROL, "Mosquitto SetReady()\n");
    if (!m_canProcessMessages) {
        m_canProcessMessages = true;
        mosquitto_message_callback_set(m_mosq, mosq_msg_callback);
    }

    // Register topics which were set since we originally connected
    // But only if already connected!
    if (m_isConnected) {
        HandleConnect();
    }
}

void MosquittoClient::HandleConnect() {
    m_isConnected = true;
    LogWarn(VB_CONTROL, "Mosquitto Connected\n");
    if (!m_canProcessMessages) {
        LogWarn(VB_CONTROL, "HandleConnect() called before can process messages.  Won't Register topics yet.\n");
        return;
    }

    LogInfo(VB_CONTROL, "Mosquitto Connected.... Will Subscribe to Topics\n");
    std::vector<std::string> subscribe_topics;
    subscribe_topics.push_back(m_baseTopic + "/set/#");

    m_subBaseTopic = getSetting("MQTTSubscribe");
    if (m_subBaseTopic != "") {
        LogDebug(VB_CONTROL, "MQTT Subscribing to topic: '%s'\n", m_subBaseTopic.c_str());
        subscribe_topics.push_back(m_subBaseTopic);
    }

    for (auto& topic : callbackTopics) {
        if (topic.rfind("/set/", 0) != 0) {
            std::string tp = m_baseTopic + topic;
            subscribe_topics.push_back(tp);
        }
    }

    for (auto& subscribe : subscribe_topics) {
        LogDebug(VB_CONTROL, "MQTT: Preparing to Subscribe to %s\n", subscribe.c_str());
        int rc = mosquitto_subscribe(m_mosq, NULL, subscribe.c_str(), 0);
        if (rc != MOSQ_ERR_SUCCESS) {
            LogErr(VB_CONTROL, "Error, unable to subscribe to %s: %d\n", subscribe.c_str(), rc);
        }
    }

    WarningHolder::RemoveWarning(4, "MQTT Disconnected");
    LogInfo(VB_CONTROL, "MQTT HandleConnect Complete\n");
}

void MosquittoClient::HandleDisconnect() {
    long long tm = GetTimeMS();
    LogWarn(VB_CONTROL, "Mosquitto Disconnected. Will try reconnect %ld\n", tm);
    m_isConnected = false;
    Timers::INSTANCE.addTimer("MosquittoDisconnect", tm + 10000, [this, tm]() {
        if (!m_isConnected) {
            WarningHolder::AddWarning(4, "MQTT Disconnected");
        }
    });
}

/*
 *
 */
void MosquittoClient::MessageCallback(void* obj, const struct mosquitto_message* message) {
    if (!m_canProcessMessages) {
        // FPPd is not yet in a state where incoming messages can be processed
        return;
    }
    bool match = 0;
    std::string emptyStr;
    std::string topic;
    std::string payload;

    LogDebug(VB_CONTROL, "Received Mosquitto message: '%s' on topic '%s'\n",
             (char*)message->payload, message->topic);

    if (message->topic)
        topic = (char*)message->topic;

    if (message->payload)
        payload = (char*)message->payload;

    CacheSetMessage(topic, payload);

    // If not our base, then return.
    if (topic.find(m_baseTopic) != 0) {
        // Warn if we are not subscribing to any other topics
        if (m_subBaseTopic == "")
            LogWarn(VB_CONTROL, "Topic '%s' doesn't match base. How is that possible?\n", message->topic);

        return;
    }

    for (auto& a : callbackTopics) {
        std::string s = m_baseTopic + a;
        LogDebug(VB_CONTROL, "Testing Callback '%s'\n", s.c_str());
        mosquitto_topic_matches_sub(s.c_str(), message->topic, &match);
        if (match) {
            std::string tp = message->topic;
            tp.replace(0, m_baseTopic.size(), emptyStr);
            Events::InvokeCallback(a, tp, payload);
            LogDebug(VB_CONTROL, "Found and Completed Callback for '%s'\n", s.c_str());
            return;
        }
    }

    LogWarn(VB_CONTROL, "No match found for Mosquitto topic '%s'\n",
            message->topic);
}

void MosquittoClient::CacheSetMessage(std::string& topic, std::string& message) {
    std::unique_lock<std::mutex> lock(messageCacheLock);

    LogDebug(VB_CONTROL, "MosquittoClient::CacheSetMessage('%s', '%s')\n", topic.c_str(), message.c_str());

    messageCache[topic] = message;
}

bool MosquittoClient::CacheCheckMessage(std::string& topic, std::string& message) {
    std::unique_lock<std::mutex> lock(messageCacheLock);

    auto search = messageCache.find(topic);
    if (search != messageCache.end())
        return messageCache[topic] == message;

    return false;
}

void MosquittoClient::dumpMessageCache(Json::Value& result) {
    // NOTE: This assumes that result is an Array
    std::unique_lock<std::mutex> lock(messageCacheLock);

    for (const auto& pair : messageCache) {
        result[pair.first] = pair.second;
    }
}