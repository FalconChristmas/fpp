#ifndef __SENSORS_h__
#define __SENSORS_h__


#include <jsoncpp/json/json.h>
#include <list>

class Sensor;

class Sensors {
public:
    static Sensors INSTANCE;
    
    void Init();
    void Close();
    
    
    void addSensors(Json::Value &config);
    
    void reportSensors(Json::Value &root);
    
    
private:
    Sensor* createSensor(Json::Value &s);
    std::list<Sensor*> sensors;
};




#endif
