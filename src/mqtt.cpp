/*
 *   Mosquitto Client for Falcon Player (FPP)
 *
 *   Copyright (C) 2017 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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
#include "playlist/Playlist.h"

#include <string.h>
#include <unistd.h>

#define FALCON_TOPIC "falcon/player"

MosquittoClient *mqtt = NULL;

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
  : m_port(1883),
	m_keepalive(60),
	m_mosq(NULL)
{
	LogDebug(VB_CONTROL, "MosquittoClient::MosquittoClient('%s', %d, '%s')\n",
		host.c_str(), port, topicPrefix.c_str());

	m_host = host;
	m_topicPrefix = topicPrefix;

	if (m_topicPrefix.size())
		m_baseTopic = m_topicPrefix + "/";
	
	m_baseTopic += FALCON_TOPIC;
	m_baseTopic += "/";
	m_baseTopic += getSetting("HostName");

	m_topicPlaylist = m_baseTopic + "/playlist/#";

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
int MosquittoClient::Init(void)
{
	mosquitto_lib_init();

	m_mosq = mosquitto_new(getSetting("HostName"), true, NULL);
	if (!m_mosq)
	{
		LogErr(VB_CONTROL, "Error, unable to create new Mosquitto instance.\n");
		return 0;
	}

	mosquitto_log_callback_set(m_mosq, mosq_log_callback);
	mosquitto_message_callback_set(m_mosq, mosq_msg_callback);

	int result = mosquitto_connect(m_mosq, m_host.c_str(), m_port, m_keepalive);

	if (result)
	{
		LogErr(VB_CONTROL, "Error, unable to connect to Mosquitto Broker at %s: %d\n",
			m_host.c_str(), result);
		return 0;
	}

	mosquitto_subscribe(m_mosq, NULL, FALCON_TOPIC "/#", 0);

	int loop = mosquitto_loop_start(m_mosq);
	if (loop != MOSQ_ERR_SUCCESS)
	{
		LogErr(VB_CONTROL, "Error, unable to start Mosquitto loop: %d\n", loop);
		return 0;
	}

	return 1;
}

/*
 *
 */
int MosquittoClient::PublishRaw(const std::string &topic, const std::string &msg)
{
	pthread_mutex_lock(&m_mosqLock);

	int result = mosquitto_publish(m_mosq, NULL, topic.c_str(), msg.size(), msg.c_str(), 0, 0);

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

/*
 *
 */
void MosquittoClient::MessageCallback(void *obj, const struct mosquitto_message *message)
{
	bool match = 0;
	std::string emptyStr;
	std::string topic;
	std::string payload;
	std::string hostname;

	LogDebug(VB_CONTROL, "Received Mosquitto message: '%s' on topic '%s'\n",
		(char*) message->payload, message->topic);

	hostname = getSetting("HostName");
	hostname += "/";

	if (message->topic)
		topic = (char *)message->topic;

	if (message->payload)
		payload = (char *)message->payload;

	mosquitto_topic_matches_sub(m_topicPlaylist.c_str(), message->topic, &match);

	if (match)
	{
		std::string topic(message->topic);

		if (topic.find(m_baseTopic) == 0)
		{
			topic.replace(0, m_baseTopic.size() + 1, emptyStr);
			playlist->MQTTHandler(topic, payload);
		}
		return;
	}
}
