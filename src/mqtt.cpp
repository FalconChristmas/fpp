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
#include "overlays/PixelOverlay.h"
#include "playlist/Playlist.h"

#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "commands/Commands.h"
#include "common.h"

#define FALCON_TOPIC "falcon/player"

MosquittoClient *mqtt = NULL;

class MQTTCommand : public Command {
public:
    MQTTCommand() : Command("MQTT") {
        args.push_back(CommandArg("topic", "string", "Topic"));
        args.push_back(CommandArg("message", "string", "Message"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() < 2) {
            return  std::make_unique<Command::ErrorResult>("MQTT Requires two arguments");
        }
        mqtt->PublishRaw(args[0], args[1]);
        return std::make_unique<Command::Result>("OK");
    }
};


void mosq_disc_callback(struct mosquitto *mosq, void *userdata, int level)
{
	if (userdata) {
		((MosquittoClient *)userdata)->HandleDisconnect();
	}
	/*
	 * Don't call reconnect here. Per documentation, loop_start() will handle
	 * reconnect automatically.
	 */
	//mosquitto_reconnect(mosq);
}

void mosq_connect_callback(struct mosquitto *mosq, void *userdata, int level)
{
	if (userdata) {
		((MosquittoClient *)userdata)->HandleConnect();
	}
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
    m_isConnected(false),
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

    std::function<void(const std::string &, const std::string &)> f = [] (const std::string &topic, const std::string &payload) {
        if (topic.size() <= 13) {
            Json::Value val = LoadJsonFromString(payload);
            CommandManager::INSTANCE.run(val);
        } else {
            std::vector<std::string> args;
            std::string command;
            
            std::string ntopic = topic.substr(13); //wrip off /set/command/
            args = splitWithQuotes(ntopic, '/');
            command = args[0];
            args.erase(args.begin());
            bool foundp = false;
            for (int x = 0; x < args.size(); x++) {
                if (args[x] == "{Payload}") {
                    args[x] = payload;
                    foundp = true;
                }
            }
            if (payload != "" && !foundp)  {
                args.push_back(payload);
            }
            if (args.size() == 0 && payload != "") {
                Json::Value val = LoadJsonFromString(payload);
                CommandManager::INSTANCE.run(command, val);
            } else {
                CommandManager::INSTANCE.run(command, args);
            }
        }
    };
    AddCallback("/set/command", f);
    AddCallback("/set/command/#", f);

    std::function<void(const std::string &, const std::string &)> lf = [] (const std::string &topic, const std::string &payload) {
        PixelOverlayManager::INSTANCE.LightMessageHandler(topic, payload);
    };
    AddCallback("/light/#", lf);
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
	    int digit = rand() %10;
            char digitc = '0' + digit;
	    clientId.append(1,digitc);
    	}
    }
    LogInfo(VB_CONTROL, "Using MQTT client id of %s \n", clientId.c_str());
    m_mosq = mosquitto_new(clientId.c_str(), true, NULL);
	if (!m_mosq)
	{
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
    
    CommandManager::INSTANCE.addCommand(new MQTTCommand());

    
    return 1;
}

/*
 *
 */
int MosquittoClient::PublishRaw(const std::string &topic, const std::string &msg, const int qos, const bool retain)
{
	LogDebug(VB_CONTROL, "Publishing message '%s' on topic '%s'\n", msg.c_str(), topic.c_str());

	pthread_mutex_lock(&m_mosqLock);

	int result = mosquitto_publish(m_mosq, NULL, topic.c_str(), msg.size(), msg.c_str(), qos, retain);

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
int MosquittoClient::Publish(const std::string &subTopic, const std::string &msg, const int qos, const bool retain)
{
	std::string topic = m_baseTopic + "/" + subTopic;

	return PublishRaw(topic, msg, qos, retain);
}

/*
 *
 */
int MosquittoClient::Publish(const std::string &subTopic, const int value, const int qos, const bool retain)
{
	std::string topic = m_baseTopic + "/" + subTopic;
	std::string msg = std::to_string(value);

	return PublishRaw(topic, msg, qos, retain);
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

void MosquittoClient::SetReady() {
    if (!m_canProcessMessages) {
	LogDebug(VB_CONTROL, "Mosquitto SetReady()\n");
        m_canProcessMessages = true;
        mosquitto_message_callback_set(m_mosq, mosq_msg_callback);

        
       int frequency = atoi(getSetting("MQTTFrequency"));
       if (frequency > 0) {
            // create  background Publish Thread
            int result = pthread_create(&m_mqtt_publish_t, NULL, &RunMqttPublishThread, (void*) this);
            if (result != 0) {
                LogErr(VB_CONTROL, "Unable to create background Publish thread. rc=%d", result);
            }
        }
    }
    // Register topics
    HandleConnect();
}
        
void MosquittoClient::HandleConnect() {
	if (! m_canProcessMessages) {
		// Abort registrying if ReadyNotSet
		return;
	}
	LogInfo(VB_CONTROL, "Mosquitto Connecting.... Will Subscribe to Topics\n");
	m_isConnected = true;
	std::vector<std::string> subscribe_topics;
    subscribe_topics.push_back(m_baseTopic + "/set/#");
    subscribe_topics.push_back(m_baseTopic + "/light/#");
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
}


void MosquittoClient::HandleDisconnect() 
{
	LogWarn(VB_CONTROL, "Mosquitto Disconnected. Will try reconnect\n");
	m_isConnected = false;
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
	if (! m_isConnected) {
		return;
	}
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
	LogWarn(VB_CONTROL, "Starting Publish Thread with Frequency: %d\n", frequency);
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

void MosquittoClient::AddHomeAssistantDiscoveryConfig(const std::string &component, const std::string &id, Json::Value &config)
{
    LogDebug(VB_CONTROL, "Adding Home Assistant discovery config for %s/%s\n", component.c_str(), id.c_str());
    std::string cfgTopic = getSetting("MQTTHADiscoveryPrefix");

    if (cfgTopic.empty())
        cfgTopic = "homeassistant";

    cfgTopic += "/";
    cfgTopic += component;
    cfgTopic += "/";
    cfgTopic += getSetting("HostName");
    cfgTopic += "/";
    cfgTopic += id;
    cfgTopic += "/config";

    std::string cmdTopic = mqtt->GetBaseTopic();
    cmdTopic += "/";
    cmdTopic += component;
    cmdTopic += "/";
    cmdTopic += id;

    std::string stateTopic = cmdTopic;
    stateTopic += "/state";
    cmdTopic += "/cmd";

    if (getSettingInt("MQTTHADiscoveryAddHost", 0)) {
        std::string name = getSetting("HostName");
        name += "_";
        name += id;
        config["name"] = name;
    } else {
        config["name"] = id;
    }

    config["state_topic"] = stateTopic;
    config["command_topic"] = cmdTopic;

    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "";
    std::string configStr = Json::writeString(wbuilder, config);
    //std::string configStr = SaveJsonToString(config);
    PublishRaw(cfgTopic, configStr);

    // Store a copy of this so we can detect when we remove models
    cfgTopic = component;
    cfgTopic += "/";
    cfgTopic += id;
    cfgTopic += "/config";
    Publish(cfgTopic, configStr);
}

void MosquittoClient::RemoveHomeAssistantDiscoveryConfig(const std::string &component, const std::string &id)
{
    LogDebug(VB_CONTROL, "Removing Home Assistant discovery config for %s/%s\n", component.c_str(), id.c_str());
    std::string cfgTopic = getSetting("MQTTHADiscoveryPrefix");
    if (cfgTopic.empty())
        cfgTopic = "homeassistant";

    cfgTopic += "/";
    cfgTopic += component;
    cfgTopic += "/";
    cfgTopic += getSetting("HostName");
    cfgTopic += "/";
    cfgTopic += id;
    cfgTopic += "/config";

    std::string emptyStr;
    PublishRaw(cfgTopic, emptyStr);

    // Clear our cache copy and any state message
    cfgTopic = component;
    cfgTopic += "/";
    cfgTopic += id;

    std::string stateTopic = cfgTopic;
    stateTopic += "/state";
    cfgTopic += "/config";

    Publish(cfgTopic, emptyStr);
    Publish(stateTopic, emptyStr);
}

