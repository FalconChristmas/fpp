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
#include <list>
#include <map>
#include <string>

class Sensor;
class SensorSource {
public:
    SensorSource(Json::Value& config);
    virtual ~SensorSource() {}

    virtual void Init(std::map<int, std::function<bool(int)>>& callbacks) {}
    virtual void enable(int id) {}

    virtual void update(bool forceInstant = false) = 0;
    virtual int32_t getValue(int id) = 0;

    virtual const std::string& getID() const { return _id; }
    virtual void lockToGroup(int i){};

protected:
    std::string _id;
};

class Sensors {
public:
    static Sensors INSTANCE;
    Sensors();
    ~Sensors();

    void Init(std::map<int, std::function<bool(int)>>& callbacks);
    void Close();

    void DetectHWSensors();
    void addSensors(Json::Value& config);
    void reportSensors(Json::Value& root);

    void addSensorSources(Json::Value& config);

    void lockToGroup(int i);
    void updateSensorSources(bool forceInstant = false);
    SensorSource* getSensorSource(const std::string& name);

private:
    Sensor* createSensor(Json::Value& s);
    std::list<Sensor*> sensors;
    std::list<SensorSource*> sensorSources;
};
