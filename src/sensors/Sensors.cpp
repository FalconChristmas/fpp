#include "Sensors.h"


#include <iomanip>
#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>
#include <stdio.h>
#include <string.h>

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
        if (s.isMember("valueType")) {
            valueType = s["valueType"].asString();
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

#define MAX_AIN  8
#define BUF_LEN  8
class AINSensor : public Sensor {
    static int countAIN;
    static bool enabled[MAX_AIN];
    static int enabledCount;
    static int dataBufferSize;
    static char *dataBuffer;
    static unsigned short enabledIndexes[MAX_AIN];
    static unsigned short curValues[MAX_AIN];
    static volatile int file;
    static volatile int errcount;
public:
    //read all the analogs in one shot, it grabs a buffer of multiple samples and averages them
    static void readValues() {
        if (countAIN == 0) {
            return;
        }
        if (file == -1 && errcount < 10) {
            char buf[512];
            char path[256];
            if (!FileExists("/sys/bus/iio/devices/iio:device0/buffer/enable")) {
                errcount++;
                return;
            }
            //turn off the buffer so we can reconfigure
            int befile = open("/sys/bus/iio/devices/iio:device0/buffer/enable", O_RDWR);
            write(befile, "0", 1);
            close(befile);

            for (int x = 0; x < MAX_AIN; x++) {
                sprintf(path, "/sys/bus/iio/devices/iio:device0/scan_elements/in_voltage%d_en", x);
                //need to use low level calls for reading from sysfs
                //to avoid any buffering that ifstream does
                if (!FileExists(path)) {
                    continue;
                }
                int efile = open(path, O_RDWR);
                if (enabled[x]) {
                    write(efile, "1", 1);
                } else {
                    int i = read(efile, buf, 255);
                    buf[i] = 0;
                    if (buf[0] == '1') {
                        enabled[x] = true;
                    }
                }
                close(efile);
            }

            
            //buffer 10 samples
            befile = open("/sys/bus/iio/devices/iio:device0/buffer/length", O_RDWR);
            sprintf(buf, "%d", BUF_LEN);
            write(befile, buf, strlen(buf));
            close(befile);
            befile = open("/sys/bus/iio/devices/iio:device0/buffer/enable", O_RDWR);
            write(befile, "1", 1);
            close(befile);

            enabledCount = 0;
            for (int x = 0; x < MAX_AIN; x++) {
                if (enabled[x]) {
                    enabledIndexes[x] = enabledCount;
                    ++enabledCount;
                }
            }
            
            file = open("/dev/iio:device0", O_RDONLY | O_NONBLOCK);
            
            dataBufferSize = BUF_LEN * 2 * enabledCount;
            dataBuffer = (char*)malloc(dataBufferSize);
        }
        
        unsigned int vs[MAX_AIN] = {0,0,0,0,0,0,0,0};
        lseek(file, 0, SEEK_SET);
        int i = read(file, dataBuffer, dataBufferSize);
        if (i > 0) {
            unsigned short *v = (unsigned short *)dataBuffer;
            for (int b = 0; b < BUF_LEN; b++) {
                for (int x = 0; x < MAX_AIN; x++) {
                    if (enabled[x]) {
                        vs[x] += v[enabledIndexes[x]];
                    }
                }
                v += enabledCount;
            }
            for (int x = 0; x < MAX_AIN; x++) {
                vs[x] /= BUF_LEN;
                curValues[x] = vs[x];
            }
        }
    }
    
    AINSensor(Json::Value &s) : Sensor(s) {
        ++countAIN;
        address = s["address"].asString();
        addressIdx = atoi(address.c_str());
        enabled[addressIdx] = true;
        path = s["path"].asString();
        if (s.isMember("max")) {
            max = s["max"].asDouble();
        }
        if (s.isMember("min")) {
            min = s["min"].asDouble();
        }
        if (file != -1) {
            int cf = file;
            file = -1;
            close(cf);
            if (dataBuffer) {
                free(dataBuffer);
                dataBuffer = nullptr;
            }
        }
    }
    virtual ~AINSensor() {
        --countAIN;
        enabled[addressIdx] = false;
        if (countAIN == 0) {
            close(file);
            file = -1;
            errcount = 0;
            if (dataBuffer) {
                free(dataBuffer);
                dataBuffer = nullptr;
            }
        }
    }

    virtual double getValue() override {
        double d = curValues[addressIdx];
        d /= 4096;  //12 bit a2d
        d *= (max - min + 1);
        d += min;
        return d;
    }

    
    std::string address;
    std::string path;
    std::string driver;
    int addressIdx;
    double min = 0.0;
    double max = 100.0;
    
};
int AINSensor::countAIN = 0;
int AINSensor::enabledCount = 0;
bool AINSensor::enabled[MAX_AIN] = {false, false, false, false, false, false, false, false};
unsigned short AINSensor::enabledIndexes[MAX_AIN] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned short AINSensor::curValues[MAX_AIN] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile int AINSensor::file = -1;
volatile int AINSensor::errcount = 0;
int AINSensor::dataBufferSize = 0;
char *AINSensor::dataBuffer = nullptr;


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
        AINSensor::readValues();
        Json::Value &s = root["sensors"];
        for (auto a : sensors) {
            a->report(s);
        }
    }
}

