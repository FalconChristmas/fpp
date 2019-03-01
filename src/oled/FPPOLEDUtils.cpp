#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>
#include <strings.h>
#include <fstream>
#include <sstream>

#include <fcntl.h>
#include <poll.h>

#include "common.h"
#include "I2C.h"
#include "SSD1306_OLED.h"

#include "FPPOLEDUtils.h"
#include "channeloutput/BBBUtils.h"

#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <jsoncpp/json/json.h>

static inline int iw_get_ext(
           int            skfd,        /* Socket to the kernel */
           const char *   ifname,      /* Device name */
           int            request,     /* WE ID */
           struct iwreq * pwrq)        /* Fixed part of the request */
{
    strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
    return(ioctl(skfd, request, pwrq));
}

#define MAX_PAGE 2

int FPPOLEDUtils::getSignalStrength(char *iwname) {
    
    int sigLevel = 0;

    iw_statistics stats;
    int           has_stats;
    iw_range      range;
    int           has_range;

    
    struct iwreq  wrq;
    char  buffer[std::max(sizeof(iw_range), sizeof(iw_statistics)) * 2];    /* Large enough */
    iw_range *range_raw;
    
    /* Cleanup */
    bzero(buffer, sizeof(buffer));
    
    wrq.u.data.pointer = (caddr_t) buffer;
    wrq.u.data.length = sizeof(buffer);
    wrq.u.data.flags = 0;
    if (iw_get_ext(sockfd, iwname, SIOCGIWRANGE, &wrq) < 0) {
        //no ranges
        return sigLevel;
    }
    range_raw = (iw_range *) buffer;
    memcpy((char *) &range, buffer, sizeof(iw_range));
    if (range.max_qual.qual == 0) {
        return sigLevel;
    }
    wrq.u.data.pointer = (caddr_t) &stats;
    wrq.u.data.length = sizeof(struct iw_statistics);
    wrq.u.data.flags = 1;
    strncpy(wrq.ifr_name, iwname, IFNAMSIZ);
    if (iw_get_ext(sockfd, iwname, SIOCGIWSTATS, &wrq) < 0) {
        //no stats
        return sigLevel;
    }
    
    sigLevel = stats.qual.qual;
    sigLevel *= 100;
    sigLevel /= range.max_qual.qual;
    return sigLevel;
}

static bool debug = false;

static int writer(char *data, size_t size, size_t nmemb,
                  std::string *writerData)
{
    if(writerData == NULL)
        return 0;
    
    writerData->append(data, size*nmemb);
    return size * nmemb;
}
FPPOLEDUtils::FPPOLEDUtils(int ledType)
    : _ledType(ledType), _currentTest(0), _curPage(0) {
    
    if (_ledType == 2 || _ledType == 4 || _ledType == 6) {
        setRotation(2);
    } else if (_ledType) {
        setRotation(0);
    }
    
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/api/fppd/status");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    
    //have to use a socket for ioctl
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
}
FPPOLEDUtils::~FPPOLEDUtils() {
    curl_easy_cleanup(curl);
    close(sockfd);
}

void FPPOLEDUtils::outputNetwork(int idx, int y) {
    setCursor(0,y);
    print_str(networks[idx].c_str());
    if (signalStrength[idx]) {
        y = y + 7;
        int cur = 5;
        for (int x = 0; x < 7; x++) {
            if (signalStrength[idx] > cur) {
                drawLine(127 - (x/2), y, 128, y, WHITE);
            }
            y--;
            cur += 15;
        }
    }
}

inline int getLinesPage0(std::vector<std::string> &lines,
                          Json::Value &result,
                          bool allowBlank) {
    std::string status = result["status_name"].asString();
    std::string mode = result["mode_name"].asString();
    mode[0] = toupper(mode[0]);
    std::string line = mode;
    bool isIdle = (status == "idle" || status == "testing");
    int maxLines = 6;
    if (mode != "bridge") {
        //bridge is always "idle" which isn't really true
        line += ": " + status;
    } else {
        //bridge mode doesn't output a playlist section
        isIdle = false;
        maxLines = 5;
    }
    if (line.length() > 21) {
        line.resize(21);
    }
    lines.push_back(line);

    if (!isIdle) {
        std::string line = result["current_sequence"].asString();
        if (line != "" || allowBlank) {
            lines.push_back(line);
        }
        line = result["current_song"].asString();
        if (line != "" || allowBlank) {
            lines.push_back(line);
        }
        line = result["time_elapsed"].asString();
        if (line != "" || allowBlank) {
            if (line != "") {
                line = "Elapsed: " + line;
            }
            lines.push_back(line);
        }
        line = result["time_remaining"].asString();
        if (line != "" || allowBlank) {
            if (line != "") {
                line = "Remaining: " + line;
            }
            lines.push_back(line);
        }
        line = result["current_playlist"]["playlist"].asString();
        if (line != "" || allowBlank) {
            if (line != "") {
                line = "PL: " + line;
            }
            lines.push_back(line);
        }
    }
    return maxLines;
}
inline int getLinesPage1(std::vector<std::string> &lines,
                          Json::Value &result,
                          bool allowBlank) {
    for (int x = 0; x < result["sensors"].size(); x++) {
        std::string line = result["sensors"][x]["label"].asString() + " " +  result["sensors"][x]["formatted"].asString();
        if (result["sensors"][x]["valueType"].asString() == "Temperature") {
            line += "C";
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        return getLinesPage0(lines, result, allowBlank);
    }
    return 6;
}
void FPPOLEDUtils::doIteration(int count) {
    if (_ledType == 0) {
        return;
    }
    if ((count % 30) == 0 || networks.size() <= 1) {
        //every 30 seconds, rescan network for new connections
        fillInNetworks();
    }
    clearDisplay();
    
    int startY = 0;
    if (_ledType == 6) {
        startY++;
    }
    setTextSize(1);
    setTextColor(WHITE);
    setCursor(0,startY);
    if (networks.size() > 1) {
        int idx = count % networks.size();
        if (networks.size() == 2 && LED_DISPLAY_HEIGHT == 64) {
            idx = 0;
        }
        outputNetwork(idx, startY);
        startY += 8;
        if (LED_DISPLAY_HEIGHT == 64) {
            if (networks.size() > 1) {
                idx++;
                if (idx >= networks.size()) {
                    idx = 0;
                }
                outputNetwork(idx, startY);
            }
            startY += 8;
        }
    } else {
        if (count < 30) {
            print_str("FPP Booting...");
        } else {
            print_str("No Network");
        }
        startY += 8;
        if (LED_DISPLAY_HEIGHT == 64) {
            startY += 8;
        }
    }
    if (LED_DISPLAY_TYPE == LED_DISPLAY_TYPE_SSD1306) {
        drawLine(0, startY, 127, startY, WHITE);
        startY++;
    } else {
        drawLine(0, startY - 1, 127, startY - 1, WHITE);
    }
    
    buffer.clear();
    bool gotStatus = false;
    if (curl_easy_perform(curl) == CURLE_OK) {
        Json::Value result;
        Json::Reader reader;
        bool success = reader.parse(buffer, result);
        if (success) {
            gotStatus = true;
            setTextSize(1);
            setTextColor(WHITE);

            setCursor(0, startY);
            
            std::vector<std::string> lines;
            std::string line;
            int maxLines = 5;
            if (_curPage == 0) {
                maxLines = getLinesPage0(lines, result, LED_DISPLAY_HEIGHT == 64);
            } else {
                maxLines = getLinesPage1(lines, result, LED_DISPLAY_HEIGHT == 64);
            }
            if (maxLines > lines.size()) {
                maxLines = lines.size();
            }
            if (LED_DISPLAY_HEIGHT == 64) {
                for (int x = 0; x < maxLines; x++) {
                    setCursor(0, startY);
                    startY += 8;
                    line = lines[x];
                    if (line.length() > 21) {
                        line.resize(21);
                    }
                    print_str(line.c_str());
                }
            } else {
                setCursor(0, startY);
                int idx = count % maxLines;
                line = lines[idx];
                if (line.length() > 21) {
                    line.resize(21);
                }
                print_str(line.c_str());
                startY += 8;
                idx++;
                if (idx == maxLines) {
                    idx = 0;
                }
                setCursor(0, startY);
                line = lines[idx];
                if (line.length() > 21) {
                    line.resize(21);
                }
                print_str(line.c_str());
            }
        } else if (debug) {
            printf("Invalid json\n");
        }
    } else if (debug) {
        printf("Curl returned bad status\n");
    }
    if (!gotStatus) {
        setTextSize(1);
        setTextColor(WHITE);
        setCursor(0, startY);
        print_str("FPPD is not running..");
        startY += 8;
        if (count < 45) {
            //if less than 45 seconds since start, we'll assume we are booting up
            setCursor(10, startY);
            std::string line = "Booting.";
            int idx = count % 5;
            for (int i = 0; i < idx; i++) {
                line += ".";
            }
            print_str(line.c_str());
        }
    }
        
    Display();
}

void FPPOLEDUtils::fillInNetworks() {
    networks.clear();
    signalStrength.clear();
    
    char hostname[HOST_NAME_MAX];
    int result;
    result = gethostname(hostname, HOST_NAME_MAX);
    std::string hn = "Host: ";
    hn += hostname;
    networks.push_back(hn);
    signalStrength.push_back(0);

    //get all the addresses
    struct ifaddrs *interfaces,*tmp;
    getifaddrs(&interfaces);
    tmp = interfaces;
    char addressBuf[128];
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            if (strncmp("usb", tmp->ifa_name, 3) != 0) {
                //skip the usb* interfaces as we won't support multisync on those
                GetInterfaceAddress(tmp->ifa_name, addressBuf, NULL, NULL);
                if (strcmp(addressBuf, "127.0.0.1")) {
                    hn = tmp->ifa_name;
                    hn += ":";
                    hn += addressBuf;
                    networks.push_back(hn);
                    int i = getSignalStrength(tmp->ifa_name);
                    signalStrength.push_back(i);
                }
            }
        } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            //FIXME for ipv6 multisync
        } else {
            //printf("Not a netork: %s\n", tmp->ifa_name);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);
}
void FPPOLEDUtils::cycleTest() {
    _currentTest++;
    Json::Value val;
    val["cycleMS"] = 1000;
    val["enabled"] = 1;
    val["channelSet"] = "1-1048576";
    val["channelSetType"] = "channelRange";

    switch (_currentTest) {
        case 1:
            val["mode"] = "SingleChase";
            val["chaseSize"] = 3;
            val["chaseValue"] = 255;
            break;
        case 2:
            val["mode"] = "RGBChase";
            val["subMode"] = "RGBChase-RGBA";
            val["colorPattern"] = "FF000000FF000000FFFFFFFF";
            break;
        default:
            val["mode"] = "SingleChase";
            val["chaseSize"] = 3;
            val["chaseValue"] = 255;
            val["enabled"] = 0;
            _currentTest = 0;
            break;
    }
    
    Json::FastWriter writer;
    std::string data = writer.write(val);

    buffer.clear();
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/fppjson.php");
    data = "command=setTestMode&data=" + data;
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
}


void FPPOLEDUtils::parseInputActions(const std::string &file, std::vector<InputAction> &actions) {
    char vbuffer[256];
    if (FileExists(file)) {
        std::ifstream t(file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string config = buffer.str();
        Json::Value root;
        Json::Reader reader;
        bool success = reader.parse(buffer.str(), root);
        if (success) {
            for (int x = 0; x < root["inputs"].size(); x++) {
                InputAction action;
                action.pin = root["inputs"][x]["pin"].asString();
                action.mode = root["inputs"][x]["mode"].asString();
                action.edge = root["inputs"][x]["edge"].asString();
                action.action = root["inputs"][x]["type"].asString();
                action.actionValue = (action.edge == "falling" ? 0 : 1);
                action.lastActionTime = 0;
#ifdef PLATFORM_BBB
                printf("Configuring pin %s as input of type %s   (mode: %s)\n", action.pin.c_str(), action.action.c_str(), action.mode.c_str());

                action.file = getBBBPinByName(action.pin).configPin(action.mode, "in").setEdge(action.edge).openValueForPoll();
                //read the initial value to make sure nothing triggers at start
                lseek(action.file, 0, SEEK_SET);
                int len = read(action.file, vbuffer, 255);
                actions.push_back(action);
#endif
            }
        }
    }

}

void FPPOLEDUtils::run() {
    std::vector<InputAction> actions;
    char vbuffer[256];
    parseInputActions("/home/fpp/media/tmp/cape-inputs.json", actions);
    parseInputActions("/home/fpp/media/config/cape-inputs.json", actions);
    std::vector<struct pollfd> fdset(actions.size());
    
    if (actions.size() == 0 && _ledType == 0) {
        //no display and no actions, nothing to do.
        exit(0);
    }
    
    int count = 0;
    while (true) {
        doIteration(count);
        count++;
        if (actions.empty()) {
            sleep(1);
        } else {
            memset((void*)&fdset[0], 0, sizeof(struct pollfd) * actions.size());
            for (int x = 0; x < actions.size(); x++) {
                fdset[x].fd = actions[x].file;
                fdset[x].events = POLLPRI;
            }

            int rc = poll(&fdset[0], actions.size(), 1000);
            for (int x = 0; x < actions.size(); x++) {
                if ((fdset[x].revents & POLLPRI)) {
                    lseek(fdset[x].fd, 0, SEEK_SET);
                    int len = read(fdset[x].fd, vbuffer, 255);
                    vbuffer[len] = 0;
                    int v = atoi(vbuffer);
                    long long ntime = GetTime();
                    if ((v == actions[x].actionValue)
                        && (ntime > (actions[x].lastActionTime + 100))) {
                        actions[x].lastActionTime = ntime;
                        printf("Action: %s\n", actions[x].action.c_str());
                        if (actions[x].action == "Test"
                            || actions[x].action == "Test/Down") {
                            cycleTest();
                        } else if (actions[x].action == "Enter") {
                            _curPage++;
                            if (_curPage == MAX_PAGE) {
                                _curPage = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}


