#include "Sensors.h"


#include <iomanip>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#include<sys/ioctl.h>
#include<linux/i2c.h>
#include<linux/i2c-dev.h>
#include <thread>
#include <chrono>

#include "common.h"

#include "util/I2CUtils.h"

#ifdef PLATFORM_BBB
#define I2C_DEV 2
#else
#define I2C_DEV 1
#endif

static std::string exec(const std::string &cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

class Sensor {
public:
    explicit Sensor(Json::Value &s) : label(s["label"].asString())  {
        if (s.isMember("prefix")) {
            prefix = s["prefix"].asString();
        }
        if (s.isMember("postfix")) {
            postfix = s["postfix"].asString();
        }
        if (s.isMember("precision")) {
            postfix = s["precision"].asInt();
        }
        if (s.isMember("valueType")) {
            valueType = s["valueType"].asString();
        }
    }
    virtual ~Sensor() {}
        
    Sensor(Sensor const &) = delete;
    void operator=(Sensor const &x) = delete;
    
    virtual double getValue() = 0;
    
    void report(Json::Value &s) {
        std::unique_lock<std::mutex> lock(sensorLock);
        Json::Value v;
        v["label"] = label;
        v["prefix"] = prefix;
        v["postfix"] = postfix;
        double d = getValue();
        v["value"] = d;
        v["valueType"] = valueType;

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
    std::string valueType;
    int precision = 1;
    std::mutex sensorLock;
};

class I2CSensor : public Sensor {
public:
    explicit I2CSensor(Json::Value &s) : Sensor(s), address(s["address"].asString()),
        path(s["path"].asString()), driver(s["driver"].asString()), file(-1)  {
        
        if (s.isMember("multiplier")) {
            multiplier = s["multiplier"].asDouble();
        }
        int bus = I2C_DEV;
        if (!FileExists(path)) {
            //path doesn't exist, need to enable
            if (bus == 2 && !HasI2CDevice(bus)) {
                //wasn't found on the default bus of the BBB, we can
                //try the other i2c bus
                bus = 1;
                size_t start_pos = path.find("/i2c-2");
                if (start_pos != std::string::npos) {
                    path.replace(start_pos, 6, "/i2c-1");
                }
            }
            if (HasI2CDevice(bus)) {
                std::ofstream out("/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/new_device");
                out << driver << " " << address;
                out.close();
            }
        }
        int count = 0;
        while (count < 5 && !CheckForFile(path)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            count++;
        }
        //after 0.5 seconds, still doesn't exist
        if (CheckForFile(path)) {
            file = open(path.c_str(), O_RDONLY);
        } else if (HasI2CDevice(bus) && driver == "lm75") {
            //Kernel 4.19 on Pi seems to not like the lm75 chips, we'll need to read the values directly
            //switch to rawIO
            int add = strtol(address.c_str(), nullptr, 16);
            i2cUtils = new I2CUtils(bus, add);
        }
    }
    bool CheckForFile(std::string &path) {
        if (path.find("hwmon0") == std::string::npos) {
            return FileExists(path);
        }
        // if it's a hwmon path, it MAY be on a different hwmon depending on the order that hwmon
        // loads the drivers
        std::string base = path.substr(0, path.find("hwmon0"));
        std::string next = path.substr(path.find("hwmon0") + 6);
        if (!DirectoryExists(base.c_str())) {
            return false;
        }
        for (int x = 0; x < 10; x++) {
            std::string npath = base + "hwmon" + std::to_string(x) + next;
            if (FileExists(npath)) {
                path = npath;
                return true;
            }
        }
        return false;
    }
    
    virtual ~I2CSensor() {
        if (file != -1) {
            close(file);
        }
        if (i2cUtils) {
            delete i2cUtils;
        }
    }
    bool HasI2CDevice(int i2cBus) {
        char buf[256];
        sprintf(buf, "i2cdetect -y -r %d %s %s", i2cBus, address.c_str(), address.c_str());
        std::string result = exec(buf);
        return result.find("--") == std::string::npos;
    }
    
    virtual double getValue() override {
        if (i2cUtils) {
            int ret = i2cUtils->readWordData(0);
            if (ret >= 0) {
                int t = (ret >> 8) & 0xFF;
                t |= (ret << 8) & 0xFF00;
                if (t & 0x8000) {
                    //negative temperature
                    t |= 0xFFFF0000;
                }
                t = t >> 5;
                double d  = t;
                d *= 0.125;
                return d;
            }
        } else if (file != -1) {
            lseek(file, 0, SEEK_SET);
            char buffer[20];
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

    I2CUtils *i2cUtils = nullptr;
    volatile int file;
};

class AINSensor : public Sensor {
public:
    explicit AINSensor(Json::Value &s) : Sensor(s), errcount(0), address(s["address"].asString()),
        path(s["path"].asString()), file(-1) {
        
        if (s.isMember("max")) {
            max = s["max"].asDouble();
        }
        if (s.isMember("min")) {
            min = s["min"].asDouble();
        }
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
            char buffer[20];
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

class ThermalSensor : public Sensor {
public:
    explicit ThermalSensor(Json::Value &s) : Sensor(s), path(s["path"].asString()) {
        file = open(path.c_str(), O_RDONLY);
    }
    virtual ~ThermalSensor() {
        close(file);
    }
    
    virtual double getValue() override {
        if (file != -1) {
            lseek(file, 0, SEEK_SET);
            char buffer[20];
            int i = read(file, buffer, 20);
            buffer[i] = 0;
            double d = atof(buffer);
            
            d /= 1000;  //12 bit a2d
            return d;
        }
        
        return 0.0;
    }
    
    
    volatile int file;
    std::string path;
};

Sensors Sensors::INSTANCE;

void Sensors::Init() {
    int i = 0;
    char path[256] = {0};
    sprintf(path, "/sys/class/thermal/thermal_zone%d/temp", i);
    while (FileExists(path)) {
        Json::Value v;
        v["path"] = path;
        v["valueType"] = "Temperature";
        
        sprintf(path, "/sys/class/thermal/thermal_zone%d/type", i);
        if (FileExists(path)) {
            int file = open(path, O_RDONLY);
            int r = read(file, path, 30);
            path[r] = 0;
            r = 0;
            while (path[r]) {
                if (path[r] == '-') {
                    path[r] = 0;
                } else if (path[r] == '\n') {
                    path[r] = 0;
                } else {
                    path[r] = toupper(path[r]);
                    r++;
                }
            }
            std::string label = path;
            label += ": ";
            v["label"] = label;
            close(file);
        } else {
            v["label"] = "CPU: ";
        }
        sensors.push_back(new ThermalSensor(v));
        i++;
        sprintf(path, "/sys/class/thermal/thermal_zone%d/temp", i);
    }

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

