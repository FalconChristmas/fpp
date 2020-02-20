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

#ifndef _MQTT_H
#define _MQTT_H

#include <pthread.h>
#include <string>
#include <map>
#include <functional>

#include <jsoncpp/json/json.h>

#include "mosquitto.h"

void * RunMqttPublishThread(void *data);

class MosquittoClient {
  public:
  	MosquittoClient(const std::string &host, const int port, const std::string &topicPrefix);
	~MosquittoClient();

	int  Init(const std::string &username, const std::string &password, const std::string &ca_file);

	int  PublishRaw(const std::string &topic, const std::string &msg, const int qos = 1, const bool retain = true);
	int  Publish(const std::string &subTopic, const std::string &msg, const int qos = 1, const bool retain = true);
	int  Publish(const std::string &subTopic, const int value, const int qos = 1, const bool retain = true);

	void LogCallback(void *userdata, int level, const char *str);
	void MessageCallback(void *obj, const struct mosquitto_message *message);
    
	void AddCallback(const std::string &topic, std::function<void(const std::string &topic, const std::string &payload)> &callback);
	void HandleDisconnect();
	void HandleConnect();

	void PublishStatus();
	void SetReady();

	std::string GetBaseTopic() { return m_baseTopic; }

	void AddHomeAssistantDiscoveryConfig(const std::string &component, const std::string &id, Json::Value &config);
    void RemoveHomeAssistantDiscoveryConfig(const std::string &component, const std::string &id);

  private:
	bool        m_canProcessMessages;
	bool        m_isConnected;
	std::string m_host;
	int         m_port;
	int         m_keepalive;

	std::string m_topicPrefix;
	std::string m_baseTopic;

	struct mosquitto *m_mosq;
	pthread_mutex_t   m_mosqLock;
	pthread_t         m_mqtt_publish_t;
    
    std::map<std::string, std::function<void(const std::string &topic, const std::string &payload)>> callbacks;
};

extern MosquittoClient *mqtt;

#endif
