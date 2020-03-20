/*
 *   Pixel Overlay handler for Falcon Player (FPP)
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

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include <Magick++.h>
#include <magick/type.h>
#include <jsoncpp/json/json.h>

#include <chrono>

#include "effects.h"
#include "channeloutput/channeloutputthread.h"
#include "mqtt.h"
#include "common.h"
#include "settings.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"
#include "PixelOverlayModel.h"
#include "commands/Commands.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

class OverlayRange {
public:
    OverlayRange(int s, int e, int v) : start(s), end(e), value(v) {}
    
    int start = 0;
    int end = 0;
    int value = 0;
};

uint32_t PixelOverlayManager::mapColor(const std::string &c) {
    if (c[0] == '#') {
        std::string color = "0x" + c.substr(1);
        return std::stoul(color, nullptr, 0);
    } else if (c == "red") {
        return 0xFF0000;
    } else if (c == "green") {
        return 0x00FF00;
    } else if (c == "blue") {
        return 0x0000FF;
    } else if (c == "yellow") {
        return 0xFFFF00;
    } else if (c == "black") {
        return 0x000000;
    } else if (c == "white") {
        return 0xFFFFFF;
    } else if (c == "cyan") {
        return 0x00FFFF;
    } else if (c == "magenta") {
        return 0xFF00FF;
    } else if (c == "gray") {
        return 0x808080;
    } else if (c == "grey") {
        return 0x808080;
    }
    return std::stoul(c, nullptr, 0);
}


PixelOverlayManager::PixelOverlayManager() : numActive(0) {
}
PixelOverlayManager::~PixelOverlayManager() {
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
    for (auto a : models) {
        delete a.second;
    }
    models.clear();
}
void PixelOverlayManager::Initialize() {
    loadModelMap();
    RegisterCommands();
    if (getSettingInt("MQTTHADiscovery", 0))
        SendHomeAssistantDiscoveryConfig();
}

void PixelOverlayManager::loadModelMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::loadModelMap()\n");
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        updates.clear();
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
    for (auto a : models) {
        delete a.second;
    }
    models.clear();
    ConvertCMMFileToJSON();

    char filename[1024];
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/config/model-overlays.json");
    if (FileExists(filename)) {
        Json::Value root;
        bool result = LoadJsonFromFile(filename, root);
        if (!result) {
            LogErr(VB_CHANNELOUT, "Error parsing model-overlays.json.");
            return;
        }

        const Json::Value models = root["models"];
        for (int c = 0; c < models.size(); c++) {
            PixelOverlayModel *pmodel = new PixelOverlayModel(models[c]);
            this->models[pmodel->getName()] = pmodel;
        }
    }
}
void PixelOverlayManager::ConvertCMMFileToJSON() {
    
    FILE *fp;
    char filename[1024];
    char buf[64];
    char *s;
    int startChannel;
    int channelCount;
    
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/channelmemorymaps");
    if (!FileExists(filename)) {
        return;
    }
    Json::Value result;
    
    LogDebug(VB_CHANNELOUT, "Loading Channel Memory Map data.\n");
    fp = fopen(filename, "r");
    if (fp == NULL) {
        LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
        return;
    }
    
    while (fgets(buf, 64, fp) != NULL) {
        if (buf[0] == '#') {
            // Allow # comments for testing
            continue;
        }
        Json::Value model;
        
        // Name
        s = strtok(buf, ",");
        if (s) {
            std::string modelName = s;
            model["Name"] = modelName;
        } else {
            continue;
        }
        
        // Start Channel
        s = strtok(NULL, ",");
        if (!s)  continue;
        startChannel = strtol(s, NULL, 10);
        if (startChannel <= 0) {
            continue;
        }
        model["StartChannel"] = startChannel;
        
        // Channel Count
        s=strtok(NULL,",");
        if (!s) continue;
        channelCount = strtol(s, NULL, 10);
        if (channelCount <= 0) {
            continue;
        }
        model["ChannelCount"] = channelCount;
        
        // Orientation
        s=strtok(NULL,",");
        if (!s) continue;
        model["Orientation"] = !strcmp(s, "vertical") ? "vertical" : "horizontal";

        
        // Start Corner
        s=strtok(NULL,",");
        if (!s) continue;
        model["StartCorner"] = s;
        
        
        // String Count
        s=strtok(NULL,",");
        if (!s) continue;
        model["StringCount"] = (int)strtol(s, NULL, 10);
        
        // Strands Per String
        s=strtok(NULL,",");
        if (!s) continue;
        model["StrandsPerString"] = (int)strtol(s, NULL, 10);
        result["models"].append(model);
    }
    fclose(fp);
    remove(filename);
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/config/model-overlays.json");
    SaveJsonToFile(result, filename);
}

bool PixelOverlayManager::hasActiveOverlays() {
    return numActive > 0;
}

void PixelOverlayManager::modelStateChanged(PixelOverlayModel *m, const PixelOverlayState &old, const PixelOverlayState &state) {
    if (old.getState() == 0) {
        //enabling, add
        std::unique_lock<std::mutex> lock(activeModelsLock);
        activeModels.push_back(m);
        numActive++;
    } else if (state.getState() == 0) {
        //disabling, remove
        std::unique_lock<std::mutex> lock(activeModelsLock);
        activeModels.remove(m);
        numActive--;
    }
    if (numActive > 0) {
        StartChannelOutputThread();
    }
}

void PixelOverlayManager::doOverlays(uint8_t *channels) {
    if (numActive == 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(activeModelsLock);
    for (auto m : activeModels) {
        m->doOverlay(channels);
    }
    for (auto &m: activeRanges) {
        for (int s = m.start; s <= m.end; s++) {
            channels[s] = m.value;
        }
    }
    lock.unlock();
    std::unique_lock<std::mutex> l(threadLock);
    while (!afterOverlayModels.empty()) {
        PixelOverlayModel *m = afterOverlayModels.front();
        afterOverlayModels.pop_front();
        l.unlock();
        m->updateRunningEffects();
        l.lock();
    }
}

PixelOverlayModel* PixelOverlayManager::getModel(const std::string &name) {
    auto a = models.find(name);
    if (a == models.end()) {
        return nullptr;
    }
    return a->second;
}

void PixelOverlayManager::SendHomeAssistantDiscoveryConfig() {
    std::unique_lock<std::mutex> lock(modelsLock);
    for (auto & m : models) {
        Json::Value s;

        s["schema"] = "json";
        s["qos"] = 0;
        s["brightness"] = true;
        s["rgb"] = true;
        s["effect"] = false;

        mqtt->AddHomeAssistantDiscoveryConfig("light", m.second->getName(), s);
    }
}

void PixelOverlayManager::LightMessageHandler(const std::string &topic, const std::string &payload) {
    static Json::Value cache;
    std::vector<std::string> parts = split(topic, '/'); // "/light/ModelName/cmd"

    std::string model = parts[2];
    auto m = getModel(model);

    if ((parts[3] == "config") && (!m) && (!payload.empty())) {
        mqtt->RemoveHomeAssistantDiscoveryConfig("light", model);
        return;
    }

    if (parts[3] != "cmd")
        return;

    Json::Value s = LoadJsonFromString(payload);
    if (m) {
        std::unique_lock<std::mutex> lock(modelsLock);
        std::string newState = toUpperCopy(s["state"].asString());

        if (newState == "OFF") {
            m->clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            m->setState(PixelOverlayState("Disabled"));

            if (cache.isMember(model)) {
                s = cache[model];
                s["state"] = "OFF";
            }
        } else if (newState != "ON") {
            return;
        } else {
            if (!s.isMember("color")) {
                if (cache.isMember(model)) {
                    s["color"] = cache[model]["color"];
                } else {
                    Json::Value c;
                    c["r"] = 255;
                    c["g"] = 255;
                    c["b"] = 255;
                    s["color"] = c;
                }
            }

            if (!s.isMember("brightness")) {
                if (cache.isMember(model)) {
                    s["brightness"] = cache[model]["brightness"];
                } else {
                    s["brightness"] = 255;
                }
            }

            int brightness = s["brightness"].asInt();
            int r = (int)(1.0 * s["color"]["r"].asInt() * brightness / 255);
            int g = (int)(1.0 * s["color"]["g"].asInt() * brightness / 255);
            int b = (int)(1.0 * s["color"]["b"].asInt() * brightness / 255);

            m->fillOverlayBuffer(r, g, b);
            m->flushOverlayBuffer();

            m->setState(PixelOverlayState("Enabled"));
        }

        cache[model] = s;

        lock.unlock();

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        std::string state = Json::writeString(wbuilder, s);
        //std::string state = SaveJsonToString(js);

        std::string topic = "light/";
        topic += model;
        topic += "/state";
        mqtt->Publish(topic, state);
    }
}

static bool isTTF(const std::string &mainStr)
{
    return (mainStr.size() >= 4) && (mainStr.compare(mainStr.size() - 4, 4, ".ttf") == 0);
}
static void findFonts(const std::string &dir, std::map<std::string, std::string> &fonts) {
    DIR *dp;
    struct dirent *ep;
    
    dp = opendir(dir.c_str());
    if (dp != NULL) {
        while (ep = readdir(dp)) {
            int location = strstr(ep->d_name, ".") - ep->d_name;
            // We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
                continue;
            }
            
            struct stat statbuf;
            std::string dname = dir;
            dname += ep->d_name;
            lstat(dname.c_str(), &statbuf);
            if (S_ISLNK(statbuf.st_mode)) {
                //symlink, skip
                continue;
            } else if (S_ISDIR(statbuf.st_mode)) {
                findFonts(dname + "/", fonts);
            } else if (isTTF(ep->d_name)) {
                std::string fname = ep->d_name;
                fname.resize(fname.size() - 4);
                fonts[fname] = dname;
            }
        }
        closedir(dp);
    }
}

void PixelOverlayManager::loadFonts() {
    if (!fontsLoaded) {
        long unsigned int i = 0;
        char **mlfonts = MagickLib::GetTypeList("*", &i);
        for (int x = 0; x < i; x++) {
            fonts[mlfonts[x]] = "";
            free(mlfonts[x]);
        }
        findFonts("/usr/share/fonts/truetype/", fonts);
        findFonts("/usr/local/share/fonts/", fonts);
        free(mlfonts);
        fontsLoaded = true;
    }
}
 
const std::string &PixelOverlayManager::mapFont(const std::string &f) {
    if (fonts[f] != "") {
        return fonts[f];
    }
    return f;
}

const std::shared_ptr<httpserver::http_response> PixelOverlayManager::render_GET(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    int plen = req.get_path_pieces().size();
    if (p1 == "models") {
        Json::Value result;
        bool empty = true;
        if (plen == 1) {
            std::unique_lock<std::mutex> lock(modelsLock);
            bool simple = false;
            if (req.get_arg("simple") == "true") {
                simple = true;
            }
            for (auto & m : models) {
                if (simple) {
                    result.append(m.second->getName());
                    empty = false;
                } else {
                    Json::Value model;
                    m.second->toJson(model);
                    result.append(model);
                    empty = false;
                }
            }
        } else {
            std::string model = req.get_path_pieces()[1];
            std::string type;
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(model);
            if (m) {
                m->toJson(result);
            } else {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Model not found: " + req.get_path_pieces()[1], 404));
            }
        }
        if (empty && plen == 1) {
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("[]", 200, "application/json"));
        } else {
            std::string resultStr = SaveJsonToString(result, "");
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
        }
    } else if (p1 == "overlays") {
        std::string p2 = req.get_path_pieces()[1];
        std::string p3 = req.get_path_pieces().size() > 2 ? req.get_path_pieces()[2] : "";
        std::string p4 = req.get_path_pieces().size() > 3 ? req.get_path_pieces()[3] : "";
        Json::Value result;
        if (p2 == "fonts") {
            loadFonts();
            for (auto & a : fonts) {
                result.append(a.first);
            }
        } else if (p2 == "models") {
            std::unique_lock<std::mutex> lock(modelsLock);
            for (auto & m : models) {
                Json::Value model;
                m.second->toJson(model);
                model["isActive"] = (int)m.second->getState().getState();
                result.append(model);
            }
        } else if (p2 == "model") {
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(p3);
            if (m) {
                if (p4 == "data") {
                    Json::Value data;
                    m->getDataJson(data);
                    result["data"] = data;
                    result["isLocked"] = m->getRunningEffect() != nullptr; //compatibility
                    result["effectRunning"] = m->getRunningEffect() != nullptr;
                } else if (p4 == "clear") {
                    m->clear();
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                } else {
                    m->toJson(result);
                    result["isActive"] = (int)m->getState().getState();
                    if (m->getRunningEffect()) {
                        result["effectName"] = m->getRunningEffect()->name();
                        result["isLocked"] = true;
                        result["effectRunning"] = true;
                    } else {
                        result["isLocked"] = false; //compatibility
                        result["effectRunning"] = false;
                    }
                    result["width"] = m->getWidth();
                    result["height"] = m->getHeight();
                }
            }
        } else if (p2 == "effects") {
            if (p3 == "") {
                for (auto &a : PixelOverlayEffect::GetPixelOverlayEffects()) {
                    result.append(a);
                }
            } else {
                PixelOverlayEffect *e = PixelOverlayEffect::GetPixelOverlayEffect(p3);
                if (e) {
                    result = e->getDescription();
                }
            }
        }
        std::string resultStr = SaveJsonToString(result, "");
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not found: " + p1, 404));
}
const std::shared_ptr<httpserver::http_response> PixelOverlayManager::render_POST(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "models") {
        std::string p2 = req.get_path_pieces().size() > 1 ? req.get_path_pieces()[1] : "";
        if (p2 == "raw") {
            //upload of raw file
            char filename[512];
            strcpy(filename, getMediaDirectory());
            strcat(filename, "/channelmemorymaps");
            
            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Could not open Channel Memory Map config file", 500));
            }
            write(fp, req.get_content().c_str(), req.get_content().length());
            close(fp);
            std::unique_lock<std::mutex> lock(modelsLock);
            loadModelMap();
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
        } else if (req.get_path_pieces().size() == 1) {
            char filename[512];
            strcpy(filename, getMediaDirectory());
            strcat(filename, "/config/model-overlays.json");
            
            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Could not open Channel Memory Map config file", 500));
            }
            write(fp, req.get_content().c_str(), req.get_content().length());
            close(fp);
            std::unique_lock<std::mutex> lock(modelsLock);
            loadModelMap();
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
        }
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("POST Not found " + req.get_path(), 404));
}
const std::shared_ptr<httpserver::http_response> PixelOverlayManager::render_PUT(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    std::string p2 = req.get_path_pieces().size() > 1 ? req.get_path_pieces()[1] : "";
    std::string p3 = req.get_path_pieces().size() > 2 ? req.get_path_pieces()[2] : "";
    std::string p4 = req.get_path_pieces().size() > 3 ? req.get_path_pieces()[3] : "";
    if (p1 == "overlays") {
        if (p2 == "model") {
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(p3);
            if (m) {
                if (p4 == "state") {
                    Json::Value root;
                    if (LoadJsonFromString(req.get_content(), root)) {
                        if (root.isMember("State")) {
                            m->setState(PixelOverlayState(root["State"].asInt()));
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                        } else {
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Invalid request " + req.get_content(), 500));
                        }
                    } else {
                        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Could not parse request " + req.get_content(), 500));
                    }
                } else if (p4 == "fill") {
                    Json::Value root;
                    if (LoadJsonFromString(req.get_content(), root)) {
                        if (root.isMember("RGB")) {
                            m->fillOverlayBuffer(root["RGB"][0].asInt(),
                                                 root["RGB"][1].asInt(),
                                                 root["RGB"][2].asInt());
                            m->flushOverlayBuffer();
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                        } else if (root.isMember("Value")) {
                            m->fillOverlayBuffer(root["Value"].asInt(),
                                                 root["Value"].asInt(),
                                                 root["Value"].asInt());
                            m->flushOverlayBuffer();
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                        }
                    }
                } else if (p4 == "pixel") {
                    Json::Value root;
                    if (LoadJsonFromString(req.get_content(), root)) {
                        if (root.isMember("RGB")) {
                            m->setPixelValue(root["X"].asInt(),
                                             root["Y"].asInt(),
                                             root["RGB"][0].asInt(),
                                             root["RGB"][1].asInt(),
                                             root["RGB"][2].asInt());
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                        }
                    }
                } else if (p4 == "text") {
                    loadFonts();
                    Json::Value root;
                    if (LoadJsonFromString(req.get_content(), root)) {
                        if (root.isMember("Message")) {
                            std::string color = root["Color"].asString();
                            unsigned int x = mapColor(color);

                            std::string msg = root["Message"].asString();

                            std::string font = root["Font"].asString();
                            std::string position = root["Position"].asString();
                            int fontSize = root["FontSize"].asInt();
                            bool aa = root["AntiAlias"].asBool();
                            int pps = root["PixelsPerSecond"].asInt();
                            bool autoEnable = false;
                            if (root.isMember("AutoEnable")) {
                                autoEnable = root["AutoEnable"].asBool();
                            }
                            
                            std::string f = fonts[font];
                            if (f == "") {
                                f = font;
                            }
                            
                            std::vector<std::string> args;
                            args.push_back(p3);
                            args.push_back(autoEnable ? "true" : "false");
                            args.push_back("Text");
                            args.push_back(color);
                            args.push_back(font);
                            args.push_back(std::to_string(fontSize));
                            args.push_back(aa ? "true" : "false");
                            args.push_back(position);
                            args.push_back(std::to_string(pps));
                            args.push_back("0");
                            args.push_back(msg);
                            lock.unlock();
                            CommandManager::INSTANCE.run("Overlay Model Effect", args);
                            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                        }
                    }
                } else {
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Model Command Not found " + p4, 404));
                }
            } else {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Model Not found " + p3, 404));
            }
        } else if (p2 == "range") {
            int val = -1;
            bool deleteAll = false;
            if (p4 == "") {
                Json::Value root;
                if (LoadJsonFromString(req.get_content(), root)) {
                    if (root.isMember("delete") && root["delete"].asBool()) {
                        val = -1;
                    } else if (root.isMember("deleteAll") && root["deleteAll"].asBool()) {
                        std::unique_lock<std::mutex> lock(activeModelsLock);
                        int sz = activeRanges.size();
                        activeRanges.clear();
                        numActive -= sz;
                        lock.unlock();
                        if (numActive == 0) {
                            numActive++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(125));
                            numActive--;
                        }
                        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
                    } else if (root.isMember("Value")) {
                        val = root["Value"].asInt();
                    }
                }
            } else if (p4 != "delete") {
                val = std::stoi(p4, nullptr, 10);
            }

            while (p3.size()) {
                std::size_t pos = 0;
                int start = std::stoi(p3, &pos, 10);
                int end = start;
                p3 = p3.substr(pos);
                if (p3.size() && p3[0] == '-') {
                    p3 = p3.substr(1);
                    end = std::stoi(p3, &pos, 10);
                    p3 = p3.substr(pos);
                }
                int startActive = numActive;
                if (start > 0 && end >= start) {
                    end--;
                    start--;
                    std::unique_lock<std::mutex> lock(activeModelsLock);
                    auto it = activeRanges.begin();
                    bool found = false;
                    while (it != activeRanges.end()) {
                        if (it->start == start && it->end == end) {
                            found = true;
                            if (val >= 0) {
                                it->value = val;
                                it++;
                            } else {
                                activeRanges.erase(it);
                                it = activeRanges.begin();
                                numActive--;
                            }
                        } else {
                            it++;
                        }
                    }
                    if (!found) {
                        activeRanges.push_back(OverlayRange(start, end, val));
                        numActive++;
                    }
                }
                //skip the ','
                if (p3.size()) {
                    p3 = p3.substr(1);
                }
                if (numActive == 0 && startActive) {
                    //removed some, may need to wait to make sure the new values are output
                    numActive++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(125));
                    numActive--;
                }
            }
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("{ \"Status\": \"OK\", \"Message\": \"\"}", 200));
        }
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("PUT Not found " + req.get_path(), 404));
}


class OverlayCommand : public Command {
public:
    OverlayCommand(const std::string &s, PixelOverlayManager *m) : Command(s), manager(m) {}
    virtual ~OverlayCommand() {}
    
    
    std::mutex &getLock() { return manager->modelsLock;}
    PixelOverlayManager *manager;
};

class EnableOverlayCommand : public OverlayCommand {
public:
    EnableOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model State", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("State", "string", "State").setContentList({"Disabled", "Enabled", "Transparent", "TransparentRGB"}));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            m->setState(PixelOverlayState(args[1]));
            return std::make_unique<Command::Result>("Model State Set");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class ClearOverlayCommand : public OverlayCommand {
public:
    ClearOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Clear", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 1) {
            return std::make_unique<Command::ErrorResult>("Command needs 1 argument, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            m->clear();
            return std::make_unique<Command::Result>("Model Cleared");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class FillOverlayCommand : public OverlayCommand {
public:
    FillOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Fill", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("State", "string", "State").setContentList({"Don't Set", "Enabled", "Transparent", "TransparentRGB"}));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() <  2 || args.size() > 3) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 or 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            std::string color;
            std::string state = "Don't Set";
            if (args.size() == 2) {
                color = args[1];
            } else {
                state = args[1];
                color = args[2];
            }
            if (state != "Don't Set") {
                m->setState(PixelOverlayState(state));
            }
            unsigned int x = PixelOverlayManager::mapColor(color);
            m->fillOverlayBuffer((x >> 16) & 0xFF,
                                 (x >> 8) & 0xFF,
                                 x & 0xFF);
            m->flushOverlayBuffer();
            return std::make_unique<Command::Result>("Model Filled");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class TextOverlayCommand : public OverlayCommand {
public:
    TextOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Text", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Font", "string", "Font").setContentListUrl("api/overlays/fonts", false));
        args.push_back(CommandArg("FontSize", "int", "FontSize").setRange(4, 100).setDefaultValue("18"));
        args.push_back(CommandArg("FontAntiAlias", "bool", "Anti-Aliased").setDefaultValue("false"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({"Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom"}));
        args.push_back(CommandArg("Speed", "int", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("AutoEnable", "bool", "Auto Enable/Disable Model").setDefaultValue("false"));
        
        // keep text as last argument if possible as the MQTT commands will, by default, use the payload of the mqtt
        // msg as the last argument.  Thus, this allows all of the above to be topic paths, but the text to be
        // sent in the payload
        args.push_back(CommandArg("Text", "string", "Text").setAdjustable());
    }
    virtual bool hidden() const override { return true; }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() < 8) {
            return std::make_unique<Command::ErrorResult>("Command needs 9 arguments, found " + std::to_string(args.size()));
        }
        std::vector<std::string> newArgs;
        newArgs.push_back(args[0]); //model
        if (args.size() == 9) {
            newArgs.push_back(args[7]);
        } else {
            newArgs.push_back("true");
        }
        newArgs.push_back("Text");
        for (int x = 1; x < 7; x++) {
            newArgs.push_back(args[x]);
        }
        newArgs.push_back("0"); //duration
        if (args.size() == 9) {
            newArgs.push_back(args[8]);
        } else {
            newArgs.push_back(args[7]);
        }
        return CommandManager::INSTANCE.run("Overlay Model Effect", args);
    }
};

class ApplyEffectOverlayCommand : public OverlayCommand {
public:
    ApplyEffectOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Effect", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("AutoEnable", "bool", "Auto Enable/Disable").setDefaultValue("false"));
        args.push_back(CommandArg("Effect", "subcommand", "Effect").setContentListUrl("api/overlays/effects/", false));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() < 3) {
            return std::make_unique<Command::ErrorResult>("Command needs at least 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            bool autoEnable = (args[1] == "true" || args[1] == "1");
            std::string effect = args[2];
            std::vector<std::string> newArgs(args.begin() + 3, args.end());

            if (m->applyEffect(autoEnable, effect, newArgs)) {
                return std::make_unique<Command::Result>("Model Effect Started");
            }
            return std::make_unique<Command::ErrorResult>("Could not start effect: " + effect);
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};

void PixelOverlayManager::RegisterCommands() {
    if (models.empty()) {
        return;
    }
    CommandManager::INSTANCE.addCommand(new EnableOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new FillOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new TextOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new ClearOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new ApplyEffectOverlayCommand(this));
}

void PixelOverlayManager::doOverlayModelEffects() {
    std::unique_lock<std::mutex> l(threadLock);
    while (threadKeepRunning) {
        uint32_t waitTime = 1000;
        if (!updates.empty()) {
            uint64_t curTime = GetTimeMS();
            while (!updates.empty() && updates.begin()->first <= curTime) {
                std::list<PixelOverlayModel*> models = updates.begin()->second;
                uint64_t startTime = updates.begin()->first;
                updates.erase(updates.begin());
                l.unlock();
                
                for (auto m : models) {
                    int32_t ms = m->updateRunningEffects();
                    if (ms != 0) {
                        l.lock();
                        if (ms > 0) {
                            uint64_t t = startTime + ms;
                            updates[t].push_back(m);
                        } else {
                            afterOverlayModels.push_back(m);
                        }
                        l.unlock();
                    }
                }
                l.lock();
                curTime = GetTimeMS();
            }
            if (!updates.empty()) {
                waitTime = updates.begin()->first - curTime;
            }
        }
        threadCV.wait_for(l, std::chrono::milliseconds(waitTime));
    }
}
void PixelOverlayManager::removePeriodicUpdate(PixelOverlayModel*m) {
    std::unique_lock<std::mutex> l(threadLock);
    for (auto &a : updates) {
        updates.begin()->second.remove(m);
    }
    afterOverlayModels.remove(m);
}

void PixelOverlayManager::addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel*m) {
    std::unique_lock<std::mutex> l(threadLock);
    if (updateThread == nullptr) {
        threadKeepRunning = true;
        updateThread = new std::thread(&PixelOverlayManager::doOverlayModelEffects, this);
    }
    if (initialDelayMS > 0) {
        uint64_t nextTime = GetTimeMS();
        nextTime += initialDelayMS;
        updates[nextTime].push_back(m);
    } else {
        afterOverlayModels.push_back(m);
    }
    l.unlock();
    threadCV.notify_all();
}

