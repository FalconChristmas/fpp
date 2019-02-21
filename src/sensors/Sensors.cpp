#include "Sensors.h"


#include <iomanip>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>


#include "common.h"


#ifdef PLATFORM_BBB
#define I2C_DEV_PATH "/sys/bus/i2c/devices/i2c-2/new_device"
#else
#define I2C_DEV_PATH "/sys/bus/i2c/devices/i2c-1/new_device"
#endif


class Sensor {
public:
    Sensor(Json::Value &s) {
        label = s["label"].asString();
        if (s.isMember("prefix")) {
            prefix = s["prefix"].asString();
        }
        if (s.isMember("postfix")) {
            postfix = s["postfix"].asString();
        }
        if (s.isMember("precision")) {
            postfix = s["precision"].asInt();
        }
    }
    virtual ~Sensor() {}
    virtual double getValue() = 0;
    
    void report(Json::Value &s) {
        std::unique_lock<std::mutex> lock(sensorLock);
        Json::Value v;
        v["label"] = label;
        v["prefix"] = prefix;
        v["postfix"] = postfix;
        double d = getValue();
        v["value"] = d;
        
        std::ostringstream stringStream;
        stringStream << prefix
            << std::fixed << std::setprecision( precision )
            << d
            << postfix;
        v["formatted"] = stringStream.str();
        s.append(v);
    }
    std::string label;
    std::string prefix;
    std::string postfix;
    int precision = 1;
    std::mutex sensorLock;
};

class I2CSensor : public Sensor {
public:
    I2CSensor(Json::Value &s) : Sensor(s), errcount(0) {
        address = s["address"].asString();
        path = s["path"].asString();
        driver = s["driver"].asString();
        if (s.isMember("multiplier")) {
            multiplier = s["multiplier"].asDouble();
        }
        if (!FileExists(path)) {
            //path doesn't exist, need to enable
            std::ofstream out(I2C_DEV_PATH);
            out << driver << " " << address;
            out.close();
        }
        file = -1;
    }
    virtual ~I2CSensor() {
        close(file);
    }
    virtual double getValue() override {
        if (file == -1 && errcount < 10) {
            //need to use low level calls for reading from sysfs
            //to avoid any buffering that ifstream does
            if (FileExists(path)) {
                file = open(path.c_str(), O_RDONLY);
                errcount++;
            }
        }
        if (file != -1) {
            lseek(file, 0, SEEK_SET);
            char * buffer = new char [20];
            int i = read(file, buffer, 20);
            buffer[i] = 0;
            double d = atof(buffer);
            d *= multiplier;
            return d;
        }
        return 0.0;
    }
    
    std::string address;
    std::string path;
    std::string driver;
    double multiplier = 1.0;
    
    volatile int file;
    volatile int errcount;
};

class AINSensor : public Sensor {
public:
    AINSensor(Json::Value &s) : Sensor(s), errcount(0) {
        address = s["address"].asString();
        path = s["path"].asString();
        if (s.isMember("max")) {
            max = s["max"].asDouble();
        }
        if (s.isMember("min")) {
            min = s["min"].asDouble();
        }
        file = -1;
    }
    virtual ~AINSensor() {
        close(file);
    }

    virtual double getValue() override {
        if (file == -1 && errcount < 10) {
            char path[256];
            sprintf(path, "/sys/bus/iio/devices/iio:device0/in_voltage%s_raw", address.c_str());
            //need to use low level calls for reading from sysfs
            //to avoid any buffering that ifstream does
            if (FileExists(path)) {
                file = open(path, O_RDONLY);
                errcount++;
            }
        }

        if (file != -1) {
            lseek(file, 0, SEEK_SET);
            char * buffer = new char [20];
            int i = read(file, buffer, 20);
            buffer[i] = 0;
            double d = atof(buffer);
            
            d /= 4096;  //12 bit a2d
            d *= (max - min + 1);
            d += min;
            return d;
        }

        return 0.0;
    }

    
    std::string address;
    std::string path;
    std::string driver;
    double min = 0.0;
    double max = 100.0;
    
    volatile int file;
    volatile int errcount;
};


Sensors Sensors::INSTANCE;

void Sensors::Init() {
    
}
void Sensors::Close() {
    for (auto x : sensors) {
        delete x;
    }
    sensors.clear();
}

Sensor* Sensors::createSensor(Json::Value &s) {
    if (s["type"].asString() == "i2c") {
        return new I2CSensor(s);
    } else if (s["type"].asString() == "ain") {
        return new AINSensor(s);
    }
    return nullptr;
}

void Sensors::addSensors(Json::Value &config) {
    for (int x = 0; x < config.size(); x++) {
        Json::Value v = config[x];
        Sensor *sensor = createSensor(v);
        if (sensor) {
            sensors.push_back(sensor);
        }
    }
}

void Sensors::reportSensors(Json::Value &root) {
    if (!sensors.empty()) {
        Json::Value &s = root["sensors"];
        for (auto a : sensors) {
            a->report(s);
        }
    }
}

