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

#include "log.h"
#include "mqtt.h"
#include "settings.h"
#include "events.h"
#include "effects.h"
#include "playlist/Playlist.h"

#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <jsoncpp/json/json.h>

#define FALCON_TOPIC "falcon/player"

MosquittoClient *mqtt = NULL;

void mosq_disc_callback(struct mosquitto *mosq, void *userdata, int level)
{
	mosquitto_reconnect(mosq);
}

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	mqtt->LogCallback(userdata, level, str);
}

void mosq_msg_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	mqtt->MessageCallback(userdata, message);
}


/*
 *
 */
MosquittoClient::MosquittoClient(const std::string &host, const int port,
	const std::string &topicPrefix)
  : m_port(port),
	m_keepalive(60),
	m_mosq(NULL),
    m_host(host),
    m_canProcessMessages(false),
    m_topicPrefix(topicPrefix)
{
	LogDebug(VB_CONTROL, "MosquittoClient::MosquittoClient('%s', %d, '%s')\n",
		host.c_str(), port, topicPrefix.c_str());

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
}

/*
 *
 */
MosquittoClient::~MosquittoClient()
{
	mosquitto_loop_stop(m_mosq, true);

	sleep(1);

	mosquitto_destroy(m_mosq);
	mqtt = NULL;

	mosquitto_lib_cleanup();

	pthread_mutex_destroy(&m_mosqLock);
}

/*
 *
 */
int MosquittoClient::Init(const std::string &username, const std::string &password, const std::string &ca_file)
{
	mosquitto_lib_init();
        std::string host = getSetting("HostName");
        if (host == "") {
	    host = "FPP";
        }
        m_mosq = mosquitto_new(host.c_str(), true, NULL);
	if (!m_mosq)
	{
		LogErr(VB_CONTROL, "Error, unable to create new Mosquitto instance.\n");
		return 0;
	}

	mosquitto_log_callback_set(m_mosq, mosq_log_callback);
	mosquitto_disconnect_callback_set(m_mosq, mosq_disc_callback);

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

	LogDebug(VB_CONTROL, "About to call MQTT Connect (%s, %d, %d)t\n", m_host.c_str(), m_port, m_keepalive);

	int result = mosquitto_connect(m_mosq, m_host.c_str(), m_port, m_keepalive);

	if (result) {
		LogErr(VB_CONTROL, "Error, unable to connect to Mosquitto Broker at %s: %d\n",
			m_host.c_str(), result);
		LogErr(VB_CONTROL, "MQTT Error: %s\n", strerror(result));
		return 0;
	}

    int loop = mosquitto_loop_start(m_mosq);
    if (loop != MOSQ_ERR_SUCCESS) {
        LogErr(VB_CONTROL, "Error, unable to start Mosquitto loop: %d\n", loop);
        return 0;
    }
    LogInfo(VB_CONTROL, "MQTT Sucessfully Connected\n");
    
    return 1;
}

/*
 *
 */
int MosquittoClient::PublishRaw(const std::string &topic, const std::string &msg)
{
	LogDebug(VB_CONTROL, "Publishing message '%s' on topic '%s'\n", msg.c_str(), topic.c_str());

	pthread_mutex_lock(&m_mosqLock);

	int result = mosquitto_publish(m_mosq, NULL, topic.c_str(), msg.size(), msg.c_str(), 1, 1);

	pthread_mutex_unlock(&m_mosqLock);

	if (result != 0)
	{
		LogErr(VB_CONTROL, "Error running mosquitto_publish: %d\n", result);
		return 0;
	}

	return 1;
}

/*
 *
 */
int MosquittoClient::Publish(const std::string &subTopic, const std::string &msg)
{
	std::string topic = m_baseTopic + "/" + subTopic;

	return PublishRaw(topic, msg);
}

/*
 *
 */
int MosquittoClient::Publish(const std::string &subTopic, const int value)
{
	std::string topic = m_baseTopic + "/" + subTopic;
	std::string msg = std::to_string(value);

	return PublishRaw(topic, msg);
}

/*
 *
 */
void MosquittoClient::LogCallback(void *userdata, int level, const char *str)
{
	switch (level)
	{
// FIXME, comment out the first 3 of these or tie to our own debug level
//		case MOSQ_LOG_DEBUG:
//		case MOSQ_LOG_INFO:
//		case MOSQ_LOG_NOTICE:
		case MOSQ_LOG_WARNING:
		case MOSQ_LOG_ERR:
			LogDebug(VB_CONTROL, "Mosquitto Log: %d:%s\n", level, str);
	}
}

void MosquittoClient::AddCallback(const std::string &topic, std::function<void(const std::string &topic, const std::string &payload)> &callback) {
    callbacks[topic] = callback;
    if (m_canProcessMessages) {
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

void MosquittoClient::SetReady() {
    if (!m_canProcessMessages) {
        m_canProcessMessages = true;
        mosquitto_message_callback_set(m_mosq, mosq_msg_callback);
        
        std::vector<std::string> subscribe_topics;
        subscribe_topics.push_back(m_baseTopic + "/set/#");
        subscribe_topics.push_back(m_baseTopic + "/event/#"); // Legacy
        subscribe_topics.push_back(m_baseTopic + "/effect/#"); // Legacy
        
        for (auto &a : callbacks) {
            std::string topic = a.first;
            if (topic.rfind("/set/", 0) != 0) {
                std::string tp = m_baseTopic + topic;
                subscribe_topics.push_back(tp);
            }
        }
        

        for (auto &subscribe : subscribe_topics) {
            LogDebug(VB_CONTROL, "MQTT: Preparing to Subscribe to %s\n", subscribe.c_str());
            int rc = mosquitto_subscribe(m_mosq, NULL, subscribe.c_str(), 0);
            if (rc != MOSQ_ERR_SUCCESS) {
                LogErr(VB_CONTROL, "Error, unable to subscribe to %s: %d\n", subscribe.c_str(), rc);
            }
        }
        
        int frequency = atoi(getSetting("MQTTFrequency"));
        if (frequency > 0) {
            // create  background Publish Thread
            int result = pthread_create(&m_mqtt_publish_t, NULL, &RunMqttPublishThread, (void*) this);
            if (result != 0) {
                LogErr(VB_CONTROL, "Unable to create background Publish thread. rc=%d", result);
            }
        }
    }
}

/*
 *
 */
void MosquittoClient::MessageCallback(void *obj, const struct mosquitto_message *message)
{
    if (!m_canProcessMessages) {
        // FPPd is not yet in a state where incoming messages can be processed
        return;
    }
	bool match = 0;
	std::string emptyStr;
	std::string topic;
	std::string payload;

	LogDebug(VB_CONTROL, "Received Mosquitto message: '%s' on topic '%s'\n",
		(char*) message->payload, message->topic);

	if (message->topic)
		topic = (char *)message->topic;

	if (message->payload)
		payload = (char *)message->payload;

	// If not our base, then return.
	// Would only happen if subscribe is wrong
    if (topic.find(m_baseTopic) != 0) {
		return;
	}
    
    for (auto &a : callbacks) {
        std::string s = m_baseTopic + a.first;
        mosquitto_topic_matches_sub(s.c_str(), message->topic, &match);
        if (match) {
            std::string tp = message->topic;
            tp.replace(0, m_baseTopic.size(), emptyStr);
            a.second(tp, payload);
            return;
        }
    }

	std::string eventTopic = m_baseTopic + "/set/event/#";
	mosquitto_topic_matches_sub(eventTopic.c_str(), message->topic, &match);
	if (match) {
		TriggerEventByID(payload.c_str());
		return;
	}

	// Check Legacy
	eventTopic = m_baseTopic + "/event/#";
	mosquitto_topic_matches_sub(eventTopic.c_str(), message->topic, &match);
	if (match) {
		LogDebug(VB_CONTROL, "Received deprecated MQTT Topic: '%s' \n",
			message->topic);
		TriggerEventByID(payload.c_str());
		return;
	}


	std::string effectTopic = m_baseTopic + "/set/effect/#";
	std::string effectTopicOld = m_baseTopic + "/effect/#";
	// Check normal
	mosquitto_topic_matches_sub(effectTopic.c_str(), message->topic, &match);
	if (match) {
		std::string s = m_baseTopic + "/set";
		topic.replace(0,s.size() +1, emptyStr);
	} else {
		// Check Legacy
		mosquitto_topic_matches_sub(effectTopicOld.c_str(), message->topic, &match);
		if (match) {
			LogDebug(VB_CONTROL, "Received deprecated MQTT Topic: '%s' \n",
				message->topic);
			topic.replace(0, m_baseTopic.size() + 1, emptyStr);
		}
	}


	if (match) {
		// At this point, topic has had /set removed if it was present
		if (topic == "effect/stop") {
			if (payload == "") {
				StopAllEffects();
			} else {
				StopEffect(payload.c_str());
			}

		} else if (topic == "effect/start") {
			StartEffect(payload.c_str(), 0);
		}

		return ;
	}

	LogWarn(VB_CONTROL, "No match found for Mosquitto topic '%s'\n",
		message->topic);
}

void MosquittoClient::PublishStatus(){
	Json::Value json = playlist->GetMqttStatusJSON();

	std::stringstream buffer;
	buffer << json << std::endl;
	Publish("playlist_details", buffer.str());
}

void *RunMqttPublishThread(void *data) {
	sleep(3); // Give everything time to start up

	MosquittoClient *me = (MosquittoClient *) data;
	int frequency = atoi(getSetting("MQTTFrequency"));
	if (frequency < 0) {
		frequency = 0;
	} 
	LogWarn(VB_CONTROL, "MQTT Frequency: %d\nc", frequency);
	if (frequency == 0) {
		// kill thread
		LogInfo(VB_CONTROL, "Stopping MQWTT Publish Thread as frequency is zero.\nc");
        return 0;
	}

	// Loop for ever
	while(true) {
		sleep(frequency);
		me->PublishStatus();
	}
}
