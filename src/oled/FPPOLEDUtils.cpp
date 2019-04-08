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
#include <string>
#include <algorithm>

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
    : _ledType(ledType), _currentTest(0), _curPage(0), _displayOn(true) {
    
    if (_ledType == 2 || _ledType == 4 || _ledType == 6 || _ledType == 8) {
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

int FPPOLEDUtils::getLinesPage0(std::vector<std::string> &lines,
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
    if (!_doFullStatus) {
        return 1;
    }

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
int FPPOLEDUtils::getLinesPage1(std::vector<std::string> &lines,
                          Json::Value &result,
                          bool allowBlank) {
    for (int x = 0; x < result["sensors"].size(); x++) {
        std::string line;
        if (result["sensors"][x]["valueType"].asString() == "Temperature") {
            line = result["sensors"][x]["label"].asString() + result["sensors"][x]["formatted"].asString();
            line += "C (";
            float i = result["sensors"][x]["value"].asFloat();
            i *= 1.8;
            i += 32;
            char buf[25];
            sprintf(buf, "%.1f", i);
            line += buf;
            line += "F)";
        } else {
            line = result["sensors"][x]["label"].asString() + " " +  result["sensors"][x]["formatted"].asString();
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        return getLinesPage0(lines, result, allowBlank);
    }
    return 6;
}
int FPPOLEDUtils::outputTopPart(int startY, int count) {
    setCursor(0,startY);
    if (networks.size() > 1) {
        int idx = (count / 3) % networks.size();
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
    return startY;
}
int FPPOLEDUtils::outputBottomPart(int startY, int count) {
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
                if (maxLines > 1) {
                    setCursor(0, startY);
                    line = lines[idx];
                    if (line.length() > 21) {
                        line.resize(21);
                    }
                    print_str(line.c_str());
                }
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
        if (count < 60) {
            //if less than 60 seconds since start, we'll assume we are booting up
            setCursor(10, startY);
            std::string line = "Booting.";
            int idx = count % 8;
            for (int i = 0; i < idx; i++) {
                line += ".";
            }
            print_str(line.c_str());
            startY += 8;
        } else {
            print_str("FPPD is not running...");
            startY += 8;
        }
        if (_imageWidth) {
            drawBitmap(0, startY, &_image[0], _imageWidth, _imageHeight, WHITE);
        }
    }
    return startY;
}


bool FPPOLEDUtils::doIteration(int count) {
    if (_ledType == 0) {
        return false;
    }
    bool retVal = false;
    int lastNSize = networks.size();
    if ((count % 30) == 0 || networks.size() <= 1) {
        //every 30 seconds, rescan network for new connections
        fillInNetworks();
        //if networks aren't configured or have changed, keep the oled on
        if (lastNSize != networks.size() || networks.size() <= 1) {
            retVal = true;
            _displayOn = true;
        }
    }
    clearDisplay();
    
    if (_displayOn) {
        int startY = 0;
        setTextSize(1);
        setTextColor(WHITE);
        if (_ledType != 8) {
            startY = outputTopPart(startY, count);
            if (_ledType != 7 && _ledType != 8) {
                // two color display doesn't need the separator line
                if (LED_DISPLAY_TYPE == LED_DISPLAY_TYPE_SSD1306) {
                    drawLine(0, startY, 127, startY, WHITE);
                    startY++;
                } else {
                    drawLine(0, startY - 1, 127, startY - 1, WHITE);
                }
            }
            startY = outputBottomPart(startY, count);
        } else {
            //strange case... with 2 color display flipped, we still need to keep the
            //network part in the "yellow" which is now below the main part
            outputBottomPart(0, count);
            outputTopPart(48, count);
        }
    }
    Display();
    return retVal;
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

static const std::string EMPTY_STRING = "";
const std::string &FPPOLEDUtils::InputAction::checkAction(int i, long long ntimeus) {
    for (auto &a : actions) {
        if (i <= a.actionValueMax && i >= a.actionValueMin
            && (ntimeus > a.nextActionTime)) {
            //at least 10ms since last action.  Should cover any debounce time
            a.nextActionTime = ntimeus + a.minActionInterval;
            return a.action;
        }
    }
    return EMPTY_STRING;
}
bool FPPOLEDUtils::checkStatusAbility() {
#ifdef PLATFORM_BBB
    char buf[128] = {0};
    FILE *fd = fopen("/sys/firmware/devicetree/base/model", "r");
    if (fd) {
        fgets(buf, 127, fd);
        fclose(fd);
    }
    std::string model = buf;
    if (model == "TI AM335x PocketBeagle") {
        return true;
    }
    std::string file = "/home/fpp/media/tmp/cape-info.json";
    if (FileExists(file)) {
        std::ifstream t(file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string config = buffer.str();
        Json::Value root;
        Json::Reader reader;
        bool success = reader.parse(buffer.str(), root);
        if (success) {
            if (root["id"].asString() == "Unsupported") {
                return false;
            }
            if (root["verifiedKeyId"].asString() == "dk") {
                return true;
            }
        }
    }
    return false;
#else
    return true;
#endif
}


bool FPPOLEDUtils::parseInputActions(const std::string &file, std::vector<InputAction> &actions) {
    char vbuffer[256];
    bool needsPolling = false;
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
                if (action.mode.find("gpio") != std::string::npos) {
                    std::string buttonaction = root["inputs"][x]["type"].asString();
                    std::string edge = root["inputs"][x]["edge"].asString();
                    int actionValue = (edge == "falling" ? 0 : 1);
    #ifdef PLATFORM_BBB
                    printf("Configuring pin %s as input of type %s   (mode: %s)\n", action.pin.c_str(), buttonaction.c_str(), action.mode.c_str());

                    action.file = getBBBPinByName(action.pin).configPin(action.mode, "in").setEdge(edge).openValueForPoll();
                    //read the initial value to make sure nothing triggers at start
                    lseek(action.file, 0, SEEK_SET);
                    int len = read(action.file, vbuffer, 255);
                    action.actions.push_back(FPPOLEDUtils::InputAction::Action(buttonaction, actionValue, actionValue, 10000));
                    actions.push_back(action);
    #endif
                } else if (action.mode == "ain") {
                    char path[256];
                    sprintf(path, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", root["inputs"][x]["input"].asInt());
                    if (FileExists(path)) {
                        action.file = open(path, O_RDONLY);
                        for (int a = 0; a < root["inputs"][x]["actions"].size(); a++) {
                            std::string buttonaction = root["inputs"][x]["actions"][a]["action"].asString();
                            int minValue = root["inputs"][x]["actions"][a]["minValue"].asInt();
                            int maxValue = root["inputs"][x]["actions"][a]["maxValue"].asInt();
                            printf("Configuring AIN input of type %s  with range %d-%d\n", buttonaction.c_str(), minValue, maxValue);
                            action.actions.push_back(FPPOLEDUtils::InputAction::Action(buttonaction, minValue, maxValue, 250000));
                        }
                        
                        actions.push_back(action);
                        needsPolling = true;
                    }
                }
            }
        }
    }
    fflush(stdout);
    return needsPolling;
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}
// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}
// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}
static std::vector<std::string> splitAndTrim(const std::string& s, char seperator) {
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;
    while((pos = s.find(seperator, pos)) != std::string::npos) {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );
        trim(substring);
        if (substring != "") {
            output.push_back(substring);
        }
        prev_pos = ++pos;
    }
    std::string substring = s.substr(prev_pos, pos-prev_pos);
    trim(substring);
    if (substring != "") {
        output.push_back(substring); // Last word
    }
    return output;
}
static unsigned char reverselookup[16] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

static inline uint8_t reverse(uint8_t n) {
    // Reverse the top and bottom nibble then swap them.
    return (reverselookup[n&0b1111] << 4) | reverselookup[n>>4];
}
void FPPOLEDUtils::readImage() {
    _imageWidth = 0;
    _imageHeight = 0;
    if (FileExists("/home/fpp/media/tmp/cape-image.xbm")) {
        std::ifstream file("/home/fpp/media/tmp/cape-image.xbm");
        if (file.is_open()) {
            std::string line;
            bool readingBytes = false;
            while (std::getline(file, line)) {
                trim(line);
                if (!readingBytes) {
                    if (line.find("_width") != std::string::npos) {
                        std::vector<std::string> v = splitAndTrim(line, ' ');
                        _imageWidth = std::stoi(v[2]);
                    } else if (line.find("_height") != std::string::npos) {
                        std::vector<std::string> v = splitAndTrim(line, ' ');
                        _imageHeight = std::stoi(v[2]);
                    } else if (line.find("{") != std::string::npos) {
                        readingBytes = true;
                        line = line.substr(line.find("{") + 1);
                    }
                }
                if (readingBytes) {
                    std::vector<std::string> v = splitAndTrim(line, ',');
                    for (auto &s : v) {
                        if (s.find("}") != std::string::npos) {
                            readingBytes = false;
                            break;
                        }
                        int i = std::stoi(s, 0, 16);
                        uint8_t t8 = (uint8_t)i;
                        t8 = ~t8;
                        t8 = reverse(t8);
                        _image.push_back((uint8_t)t8);
                    }
                }
            }
            file.close();
        }
    }
}

void FPPOLEDUtils::run() {
    std::vector<InputAction> actions;
    char vbuffer[256];
    bool needsPolling = parseInputActions("/home/fpp/media/tmp/cape-inputs.json", actions);
    _doFullStatus = checkStatusAbility();
    std::vector<struct pollfd> fdset(actions.size());
    
    readImage();
    
    if (actions.size() == 0 && _ledType == 0) {
        //no display and no actions, nothing to do.
        exit(0);
    }
    
    int count = 0;
    long long lastUpdateTime = 0;
    long long ntime = GetTime();
    long long lastActionTime = GetTime();
    while (true) {
        if (ntime > (lastUpdateTime + 1000000)) {
            count++;
            if (doIteration(count)) {
                lastActionTime = GetTime();
            }
            lastUpdateTime = ntime;
        }
        if (actions.empty()) {
            sleep(1);
            ntime = GetTime();
        } else {
            memset((void*)&fdset[0], 0, sizeof(struct pollfd) * actions.size());
            for (int x = 0; x < actions.size(); x++) {
                fdset[x].fd = actions[x].file;
                fdset[x].events = POLLPRI;
            }

            int rc = poll(&fdset[0], actions.size(), needsPolling ? 100 : 1000);
            ntime = GetTime();
            
            for (int x = 0; x < actions.size(); x++) {
                std::string action;
                if ((fdset[x].revents & POLLPRI)) {
                    lseek(fdset[x].fd, 0, SEEK_SET);
                    int len = read(fdset[x].fd, vbuffer, 255);
                    vbuffer[len] = 0;
                    int v = atoi(vbuffer);
                    action = actions[x].checkAction(v, ntime);
                } else if (actions[x].mode == "ain") {
                    lseek(fdset[x].fd, 0, SEEK_SET);
                    int len = read(fdset[x].fd, vbuffer, 255);
                    int v = atoi(vbuffer);
                    action = actions[x].checkAction(v, ntime);
                }
                if (action != "" && !_displayOn) {
                    //just turn the display on if button is hit
                    _displayOn = true;
                    lastUpdateTime = 0;
                    lastActionTime = ntime;
                } else if (action != "") {
                    printf("Action: %s\n", action.c_str());
                    //force immediate update
                    lastUpdateTime = 0;
                    lastActionTime = ntime;
                    if (action == "Test"
                        || action == "Test/Down") {
                        cycleTest();
                    } else if (action == "Enter") {
                        _curPage++;
                        if (_curPage == MAX_PAGE) {
                            _curPage = 0;
                        }
                    }
                }
            }
            if (ntime > (lastActionTime + 120000000)) {
                _displayOn = false;
            }
        }
    }
}


