
/*
 *   Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2019 the Falcon Player Developers
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

#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <array>
#include <thread>
#include <map>

#include <fstream>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <fcntl.h>

#include <libgen.h>
#include <string.h>
#include <dirent.h>

#include <jsoncpp/json/json.h>

#ifdef PLATFORM_BBB
#define I2C_DEV 2
#else
#define I2C_DEV 1
#endif


template< typename... Args >
std::string string_sprintf( const char* format, Args... args ) {
    int length = std::snprintf( nullptr, 0, format, args... );
    char* buf = new char[length + 1];
    std::snprintf( buf, length + 1, format, args... );
    
    std::string str( buf );
    delete[] buf;
    return std::move(str);
}

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

static bool file_exists(const std::string &f) {
    return access(f.c_str(), F_OK ) != -1;
}
static std::string trim(std::string str) {
    // remove trailing white space
    while( !str.empty() && (std::isspace( str.back() ) || str.back() == 0)) str.pop_back() ;
    
    // return residue after leading white space
    std::size_t pos = 0 ;
    while( pos < str.size() && std::isspace( str[pos] ) ) ++pos ;
    return str.substr(pos) ;
}
static std::string read_string(FILE *file, int len) {
    std::string buf;
    buf.reserve(len + 1);
    buf.resize(len);
    fread(&buf[0], 1, len, file);
    return trim(buf);
}
static void put_file_contents(const std::string &path, const uint8_t *data, int len) {
    FILE *f = fopen(path.c_str(), "wb");
    fwrite(data, 1, len, f);
    fclose(f);

    struct passwd *pwd = getpwnam("fpp");
    chown(path.c_str(), pwd->pw_uid, pwd->pw_gid);

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    chmod(path.c_str(), mode);
}
static uint8_t *get_file_contents(const std::string &path, int &len) {
    FILE *fp = fopen(path.c_str(), "rb");
    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    uint8_t *data = (uint8_t*)malloc(len);
    fread(data, 1, len, fp);
    fclose(fp);
    return data;
}

static bool waitForI2CBus(int i2cBus) {
    char buf[256];
    sprintf(buf, "/sys/bus/i2c/devices/i2c-%d/new_device", i2cBus);
    
    //wait for up to 15 seconds for the i2c bus to appear
    //if it's already there, this should be nearly immediate
    for (int x = 0; x < 1500; x++) {
        if (access(buf, F_OK ) != -1) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

static bool HasI2CDevice(int i, int i2cBus) {
    char buf[256];
    sprintf(buf, "i2cdetect -y -r %d 0x%X 0x%X", i2cBus, i, i);
    std::string result = exec(buf);
    return result.find("--") == std::string::npos;
}
static FILE * DoSignatureVerify(FILE *file, const std::string &fKeyId, const std::string &path, bool &validSignature, bool &deleteEpromFile) {
    uint8_t *b = new uint8_t[32768];
    size_t pos = ftell(file);
    int flen = std::stoi(read_string(file, 6));
    while (flen != 0) {
        int flag = std::stoi(read_string(file, 2));
        if (flag < 50) {
            flen += 64;
        }
        fseek(file, flen, SEEK_CUR);
        flen = std::stoi(read_string(file, 6));
    }
    int end = ftell(file);
    fseek(file, pos, SEEK_SET);
    int len = fread(b, 1, end - pos, file);
    fclose(file);
    
    file = fopen("/home/fpp/media/tmp/eeprom", "wb");
    fwrite(b, 1, len, file);
    fclose(file);
    delete[] b;
    
    std::string cmd = "openssl dgst -sha256 -verify /opt/fpp/scripts/keys/" + fKeyId + "_pub.pem -signature " + path + " /home/fpp/media/tmp/eeprom";
    std::string result = exec(cmd);
    validSignature = result.find("OK") != std::string::npos;
    
    file = fopen("/home/fpp/media/tmp/eeprom", "rb");
    deleteEpromFile = true;
    return file;
}
static std::string checkUnsupported(const std::string &orig, int i2cbus) {
    if (HasI2CDevice(0x3c, i2cbus)) {
        // there is an oled so some sort of cape is present, we just don't know anything about it
        return "/opt/fpp/capes/other/Unknown-eeprom.bin";
    }
    return orig;
}

static void readSettingsFile(std::vector<std::string> &lines) {
    std::string line;
    std::ifstream settingsIstream("/home/fpp/media/settings");
    if (settingsIstream.good()) {
        while (std::getline(settingsIstream, line)) {
            if (line.size() > 0) {
                lines.push_back(line);
            }
        }
    }
    settingsIstream.close();
}
static void writeSettingsFile(const std::vector<std::string> &lines) {
    std::ofstream settingsOstream("/home/fpp/media/settings");
    for (auto l : lines) {
        settingsOstream << l << "\n";
    }
    settingsOstream.close();
}
static void getFileList(const std::string &basepath, const std::string &path, std::vector<std::string> &files) {
    DIR* dir = opendir(basepath.c_str());
    if (dir) {
        files.push_back(path + "/");
        struct dirent *ep;
        while (ep = readdir(dir)) {
            std::string v = ep->d_name;
            if (v != "." && v != "..") {
                std::string newBPath = basepath + "/" + v;
                std::string newPath = path + "/" + v;
                getFileList(newBPath, newPath, files);
            }
        }
        closedir(dir);
    } else if (file_exists(basepath)) {
        files.push_back(path);
    }
}

static void disableOutputs(Json::Value &disables) {
    for (int x = 0; x < disables.size(); x++) {
        std::string file = disables[x]["file"].asString();
        std::string type = disables[x]["type"].asString();
        
        std::string fullFile = "/home/fpp/media/" + file;
        if (file_exists(fullFile)) {
            Json::Value result;
            Json::CharReaderBuilder builder;
            Json::CharReader *reader = builder.newCharReader();
            std::string errors;
            std::ifstream istream(fullFile);
            std::stringstream buffer;
            buffer << istream.rdbuf();
            istream.close();
            
            std::string str = buffer.str();
            bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &result, &errors);
            if (success) {
                bool changed = false;
                if (result.isMember("channelOutputs")) {
                    for (int co = 0; co < result["channelOutputs"].size(); co++) {
                        if (result["channelOutputs"][co]["type"].asString() == type) {
                            if (result["channelOutputs"][co]["enabled"].asInt() == 1) {
                                result["channelOutputs"][co]["enabled"] = 0;
                                changed = true;
                            }
                        }
                    }
                }
                if (changed) {
                    Json::StreamWriterBuilder wbuilder;
                    std::string resultStr = Json::writeString(wbuilder, result);
                    put_file_contents(fullFile, (const uint8_t*)resultStr.c_str(), resultStr.size());
                }
            }
        }
    }
}
static void processBootConfig(Json::Value &bootConfig) {
#if defined(PLATFORM_PI)
    const std::string fileName = "/boot/config.txt";
#elif defined(PLATFORM_BBB)
    const std::string fileName = "/boot/uEnv.txt";
#else
    //unknown platform
    const std::string fileName;
#endif

    if (fileName.empty())
        return;

    int len = 0;
    uint8_t *data = get_file_contents(fileName, len);
    if (len == 0) {
        remove("/.fppcapereboot");
        return;
    }
    std::string current = (char *)data;
    std::string orig = current;
    if (bootConfig.isMember("remove")) {
        for (int x = 0; x < bootConfig["remove"].size(); x++) {
            std::string v = bootConfig["remove"][x].asString();
            size_t pos = std::string::npos;
            while ((pos  = current.find(v) )!= std::string::npos) {
                // If found then erase it from string
                current.erase(pos, v.length());
            }
        }
    }
    if (bootConfig.isMember("append")) {
        for (int x = 0; x < bootConfig["append"].size(); x++) {
            std::string v = bootConfig["append"][x].asString();
            size_t pos = current.find(v);
            if (pos == std::string::npos) {
                // If not  found then append it
                printf("Adding config option: %s\n", v.c_str());
                current += "\n";
                current += v;
                current += "\n";
            }
        }
    }
    if (current != orig) {
        put_file_contents(fileName, (const uint8_t*)current.c_str(), current.size());
        sync();
        if (!file_exists("/.fppcapereboot")) {
            const uint8_t data[2] = {32, 0};
            put_file_contents("/.fppcapereboot", data, 1);
            sync();
            setuid(0);
            reboot(RB_AUTOBOOT);
            exit(0);
        }
    } else {
        remove("/.fppcapereboot");
    }
}
static void copyFile(const std::string &src, const std::string &target) {
    int s, t;
    s = open(src.c_str(), O_RDONLY);
    if (s == -1) {
        printf("Failed to open src %s - %s\n", src.c_str(), strerror(errno));
        return;
    }
    t = open(target.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (t == -1) {
        printf("Failed to open target %s - %s\n", src.c_str(), strerror(errno));
        close(s);
        return;
    }
    uint8_t buf[257];
    while (true) {
        int l = read(s, buf, 256);
        if (l == -1) {
            printf("Error copying file %s\n", strerror(errno));
            close(s);
            close(t);
            return;
        } else if (l == 0) {
            close(s);
            close(t);
            return;
        }
        write(t, buf, l);
    }
}
static void copyIfNotExist(const std::string &src, const std::string &target) {
    if (file_exists(target)) {
        return;
    }
    copyFile(src, target);
}
bool fpp_detectCape() {
    int bus = I2C_DEV;
    waitForI2CBus(bus);
    std::string EEPROM;
    if (bus == 2 && !HasI2CDevice(0x50, bus)) {
        printf("Did not find 0x50 on i2c2, trying i2c1.\n");
        if (HasI2CDevice(0x50, 1)) {
            bus = 1;
        }
    }
    if (HasI2CDevice(0x50, bus)) {
        EEPROM = string_sprintf("/sys/bus/i2c/devices/%d-0050/eeprom", bus);
        if (!file_exists(EEPROM)) {
            std::string newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
            int f = open(newDevFile.c_str(), O_WRONLY);
            write(f, "24c256 0x50", 11);
            close(f);
            
            for (int x = 0; x < 50; x++) {
                if (file_exists(EEPROM)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            if (!file_exists(EEPROM)) {
                //try again
                newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
                f = open(newDevFile.c_str(), O_WRONLY);
                write(f, "24c256 0x50", 11);
                close(f);
                for (int x = 0; x < 50; x++) {
                    if (file_exists(EEPROM)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
    } else {
        printf("Did not find eeprom on i2c.\n");
    }
    if (!file_exists(EEPROM)) {
        printf("EEPROM file doesn't exist %s.\n", EEPROM.c_str());
        EEPROM = "/opt/fpp-vendor/cape-eeprom.bin";
    }
    if (!file_exists(EEPROM)) {
        EEPROM = "/home/fpp/media/config/cape-eeprom.bin";
    }
    if (!file_exists(EEPROM)) {
        EEPROM = checkUnsupported(EEPROM, bus);
    }
    if (!file_exists(EEPROM)) {
        printf("Could not detect any cape\n");
        return false;
    }
    printf("Using %s\n", EEPROM.c_str());
    FILE *file = fopen(EEPROM.c_str(), "rb");
    uint8_t *buffer = new uint8_t[32768]; //32K is the largest eeprom we support, more than enough
    int l = fread(buffer, 1, 6, file);
    if (buffer[0] != 'F' || buffer[1] != 'P' || buffer[2] != 'P'
        || buffer[3] != '0' || buffer[4] != '2') {
        fclose(file);
        EEPROM = "/home/fpp/media/config/cape-eeprom.bin";
        if (file_exists(EEPROM)) {
            file = fopen(EEPROM.c_str(), "rb");
            l = fread(buffer, 1, 6, file);
        } else {
            printf("Could not detect any cape\n");
            delete [] buffer;
            return false;
        }
    }
    bool validSignature = true;
    bool deleteEpromFile = false;
    std::string fkeyId;
    std::string cape;
    std::string capev;
    std::string capesn;
    std::map<std::string, std::string> extras;

    if (buffer[0] == 'F' && buffer[1] == 'P' && buffer[2] == 'P'
        && buffer[3] == '0' && buffer[4] == '2') {
        
        cape = read_string(file, 26); // cape name + nulls
        capev = read_string(file, 10); // cape version + nulls
        capesn = read_string(file, 16); // cape serial# + nulls
        
        std::string flenStr = read_string(file, 6); //length of the section
        printf("Found cape %s, Version %s, Serial Number: %s\n", cape.c_str(), capev.c_str(), capesn.c_str());
        int flen = std::stoi(flenStr);
        while (flen) {
            int flag = std::stoi(read_string(file, 2));
            std::string path;
            if (flag < 50) {
                path = "/home/fpp/media/";
                path += read_string(file, 64);
            }
            switch (flag) {
                case 0:
                case 1:
                case 2:
                case 3: {
                    int l = fread(buffer, 1, flen, file);
                    put_file_contents(path, buffer, flen);
                    char *s1 = strdup(path.c_str());
                    std::string dir = dirname(s1);
                    free(s1);
                    if (flag == 1) {
                        std::string cmd = "cd " + dir + "; unzip " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    } else if (flag == 2) {
                        std::string cmd = "cd " + dir + "; tar -xzf " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    } else if (flag == 3) {
                        std::string cmd = "cd " + dir + "; tar -xjf " + path + " 2>&1";
                        exec(cmd);
                        unlink(path.c_str());
                    }
                    break;
                }
                case 97: {
                    std::string eKey = read_string(file, 12);
                    std::string eValue = read_string(file, flen - 12);
                    extras[eKey] = eValue;
                    break;
                }
                case 98: {
                    std::string type = read_string(file, 2);
                    if (type == "1") {
                        if (EEPROM.find("/i2c") == std::string::npos) {
                            validSignature = false;
                        }
                    } else if (type == "2") {
                        if (EEPROM.find("cape-eeprom.bin") == std::string::npos) {
                            validSignature = false;
                        }
                    } else if (type != "0") {
                        validSignature = false;
                    }
                    break;
                }
                case 99: {
                    fkeyId = read_string(file, 6);
                    fread(buffer, 1, flen - 6, file);
                    path = "/home/fpp/media/tmp/eeprom.sig";
                    put_file_contents(path, buffer, flen - 6);
                    file = DoSignatureVerify(file, fkeyId, path, validSignature, deleteEpromFile);
                    unlink(path.c_str());
                    break;
                }
                default:
                    fseek(file, flen, SEEK_CUR);
                    printf("Don't know how to handle -%s- with type %d and length %d\n", path.c_str(), flag, flen);
            }
            flenStr = read_string(file, 6); //length of the section
            flen = std::stoi(flenStr);
        }
    }
    fclose(file);
    delete [] buffer;
    if (deleteEpromFile) {
        unlink("/home/fpp/media/tmp/eeprom");
    }

    // if the cape-info has default settings and those settings are not already set, set them
    // also put the serialNumber into the cape-info for display
    if (file_exists("/home/fpp/media/tmp/cape-info.json")) {
        // We would prefer to use LoadJsonFromFile() here, but we want to
        // keep fppcapedetect small and using the helper would mean pulling
        // in common.o and other dependencies.
        Json::Value result;
        Json::CharReaderBuilder builder;
        Json::CharReader *reader = builder.newCharReader();
        std::string errors;
        std::ifstream istream("/home/fpp/media/tmp/cape-info.json");
        std::stringstream buffer;
        buffer << istream.rdbuf();
        istream.close();

        std::string str = buffer.str();
        bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &result, &errors);
        if (success) {
            for (auto kv : extras) {
                result[kv.first] = kv.second;
            }
            if (result["id"].asString() != "Unsupported" && result["id"].asString() != "Unknown") {
                result["serialNumber"] = capesn;
            }
            std::vector<std::string> lines;
            bool settingsChanged = false;
            if (!validSignature) {
                if (result.isMember("vendor")) result.removeMember("vendor");
                if (result.isMember("provides")) result.removeMember("provides");
                if (result.isMember("verifiedKeyId")) result.removeMember("verifiedKeyId");
            } else {
                result["verifiedKeyId"] = fkeyId;
                if (result.isMember("defaultSettings")) {
                    readSettingsFile(lines);
                    
                    std::map<std::string, std::string> defaults;
                    for (auto a : result["defaultSettings"].getMemberNames()) {
                        std::string v = result["defaultSettings"][a].asString();
                        bool found = false;
                        for (auto l : lines) {
                            if (l.find(a) == 0) {
                                found = true;
                            }
                        }
                        if (!found) {
                            lines.push_back(a + " = \"" + v + "\"");
                            settingsChanged = true;
                        }
                    }
                }
            }
            if (result.isMember("bootConfig")) {
                //if the cape requires changes/update to config.txt (Pi) or uEnv.txt (BBB)
                //we need to process them and see if we have to apply the changes and reboot or not
                processBootConfig(result["bootConfig"]);
            }

            if (result.isMember("removeSettings")) {
                if (lines.empty()) {
                    readSettingsFile(lines);
                }
                for (int x = 0; x < result["removeSettings"].size(); x++) {
                    std::string v = result["removeSettings"][x].asString();
                    std::string found = "";
                    for (int l = 0; l < lines.size(); l++) {
                        if (lines[l].find(v) == 0) {
                            lines.erase(lines.begin() + l);
                            settingsChanged = true;
                            break;
                        }
                    }
                }
            }
            if (result.isMember("modules")) {
                //if the cape requires kernel modules, load them at this
                //time so they will be available later
                for (int x = 0; x < result["modules"].size(); x++) {
                    std::string v = "/sbin/modprobe " + result["modules"][x].asString();
                    exec(v.c_str());
                }
            }
            if (result.isMember("copyFiles")) {
                //if the cape requires certain files copied into place (asoundrc for example)
                for (auto src : result["copyFiles"].getMemberNames()) {
                    std::string target = result["copyFiles"][src].asString();
                    
                    if (src[0] != '/') {
                        src = "/home/fpp/media/" + src;
                    }
                    if (target[0] != '/') {
                        target = "/home/fpp/media/" + target;
                    }
                    copyFile(src, target);
                }
            }
            if (result.isMember("i2cDevices")) {
                //if the cape has i2c devices on it that need to be registered, load them at this
                //time so they will be available later
                for (int x = 0; x < result["i2cDevices"].size(); x++) {
                    
                    std::string v = result["i2cDevices"][x].asString();

                    std::string newDevFile = string_sprintf("/sys/bus/i2c/devices/i2c-%d/new_device", bus);
                    int f = open(newDevFile.c_str(), O_WRONLY);
                    std::string newv = string_sprintf("%s", v.c_str());
                    write(f, newv.c_str(), newv.size());
                    close(f);
                }
            }
            if (result.isMember("disableOutputs")) {
                disableOutputs(result["disableOutputs"]);
            }
            if (settingsChanged) {
                writeSettingsFile(lines);
            }

            Json::StreamWriterBuilder wbuilder;
            std::string resultStr = Json::writeString(wbuilder, result);
            put_file_contents("/home/fpp/media/tmp/cape-info.json", (const uint8_t*)resultStr.c_str(), resultStr.size());
        } else {
            printf("Failed to parse cape-info.json\n");
        }
    }
    
    // if there are default configurations, copy them into place if they dont already exist
    if (validSignature) {
        std::vector<std::string> files;
        getFileList("/home/fpp/media/tmp/defaults", "", files);
        for (auto a : files) {
            std::string src = "/home/fpp/media/tmp/defaults" + a;
            std::string target = "/home/fpp/media" + a;
            
            if (target[target.length() - 1] == '/') {
                mkdir(target.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            } else {
                copyIfNotExist(src, target);
            }
        }
    } else {
        if (file_exists("/home/fpp/media/tmp/cape-sensors.json")) unlink("/home/fpp/media/tmp/cape-sensors.json");
        if (file_exists("/home/fpp/media/tmp/cape-inputs.json")) unlink("/home/fpp/media/tmp/cape-inputs.json");
    }

    return !validSignature;
}

