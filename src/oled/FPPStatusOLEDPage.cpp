
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
#include "SSD1306_OLED.h"


#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <jsoncpp/json/json.h>
#include "FPPStatusOLEDPage.h"
#include "FPPMainMenu.h"


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

int FPPStatusOLEDPage::getSignalStrength(char *iwname) {
    
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



FPPStatusOLEDPage::FPPStatusOLEDPage()
: _currentTest(0), _curPage(0), _doFullStatus(true), _iterationCount(0), _hasSensors(false) {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:32322/fppd/status");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    
    //have to use a socket for ioctl
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    readImage();
    mainMenu = new FPPMainMenu(this);
}
FPPStatusOLEDPage::~FPPStatusOLEDPage() {
    curl_easy_cleanup(curl);
    close(sockfd);
    delete mainMenu;
}


void FPPStatusOLEDPage::outputNetwork(int idx, int y) {
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

static bool compareLines(int x, std::vector<std::string> &lines1, std::vector<std::string> &lines2) {
    if (x < lines1.size() && x < lines2.size()) {
        return lines1[x] != lines2[x];
    } else if (x < lines1.size() || x < lines2.size()) {
        return true;
    }
    return false;
}

bool FPPStatusOLEDPage::checkIfStatusChanged(Json::Value &result) {
    std::string mode = result["mode_name"].asString();
    mode[0] = toupper(mode[0]);
    if (currentMode != mode) {
        return true;
    }

    //if the status has materially changed, turn the oled back on
    std::vector<std::string> lines;
    int maxLines = getLinesPage0(lines, result, true);
    if (compareLines(0, lines, _lastStatusLines)) {
        //main status
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(1, lines, _lastStatusLines)) {
        //running sequence
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(2, lines, _lastStatusLines)) {
        //running song
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(5, lines, _lastStatusLines)) {
        //running playlist
        _lastStatusLines = lines;
        return true;
    }

    return false;
}

int FPPStatusOLEDPage::getLinesPage0(std::vector<std::string> &lines,
                                Json::Value &result,
                                bool allowBlank) {
    std::string status = result["status_name"].asString();
    currentMode = result["mode_name"].asString();
    currentMode[0] = toupper(currentMode[0]);
    std::string line = currentMode;
    bool isIdle = (status == "idle" || status == "testing");
    int maxLines = 6;
    if (currentMode != "Bridge") {
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
    _hasSensors = result["sensors"].size() > 0;
    return maxLines;
}
int FPPStatusOLEDPage::getLinesPage1(std::vector<std::string> &lines,
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
int FPPStatusOLEDPage::outputTopPart(int startY, int count) {
    setCursor(0,startY);
    if (networks.size() > 1) {
        int idx = (count / 3) % networks.size();
        int lines = LED_DISPLAY_HEIGHT >= 64 ? 2 : 1;
        if (networks.size() <= 5 && LED_DISPLAY_HEIGHT > 65) {
            idx = 0;
            lines = networks.size() < 5 ? networks.size() : 5;
            if (lines < 2) lines = 2;
        }
        if (networks.size() <= lines) {
            idx = 0;
        }        
        outputNetwork(idx, startY);
        startY += 8;
        if (LED_DISPLAY_HEIGHT > 65) {
            startY += 2; //little extra space
        }
        for (int x = 1; x < lines; x++) {
            idx++;
            if (idx >= networks.size()) {
                idx = 0;
            }
            outputNetwork(idx, startY);
            startY += 8;
            if (LED_DISPLAY_HEIGHT > 65) {
                startY += 2; //little extra space
            }
        }
    } else {
        if (count < 40) {
            print_str("FPP Booting...");
        } else {
            print_str("No Network");
        }
        startY += 8;
        if (LED_DISPLAY_HEIGHT >= 64) {
            startY += 8;
        }
    }
    return startY;
}
bool FPPStatusOLEDPage::getCurrentStatus(Json::Value &result) {
    buffer.clear();
    bool gotStatus = false;
    if (curl_easy_perform(curl) == CURLE_OK) {
        if (LoadJsonFromString(buffer, result)) {
            std::string status = result["status_name"].asString();
            return true;
        } else if (debug) {
            printf("Invalid json\n");
        }
    } else if (debug) {
        printf("Curl returned bad status\n");
    }
    return false;
}


int FPPStatusOLEDPage::outputBottomPart(int startY, int count, bool statusValid, Json::Value &result) {
    if (statusValid) {
        setTextSize(1);
        setTextColor(WHITE);
        
        setCursor(0, startY);
        
        std::vector<std::string> lines;
        std::string line;
        int maxLines = 5;
        if (_curPage == 0) {
            maxLines = getLinesPage0(lines, result, LED_DISPLAY_HEIGHT >= 64);
        } else {
            maxLines = getLinesPage1(lines, result, LED_DISPLAY_HEIGHT >= 64);
        }
        if (maxLines > lines.size()) {
            maxLines = lines.size();
        }
        if (LED_DISPLAY_HEIGHT >= 64) {
            for (int x = 0; x < maxLines; x++) {
                setCursor(0, startY);
                startY += 8;
                if (LED_DISPLAY_HEIGHT > 65) {
                    startY += 2;
                }
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
    } else {
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
            print_str("FPPD is not running..");
            startY += 8;
        }
        if (_imageWidth) {
            int y = startY;
            if (oledType != OLEDType::TWO_COLOR) {
                --y;
            }
            if (LED_DISPLAY_HEIGHT > 65) {
                y += 4;
            }

            drawBitmap(0, y, &_image[0], _imageWidth, _imageHeight, WHITE);
        }
    }
    return startY;
}


bool FPPStatusOLEDPage::doIteration(bool &displayOn) {
    _iterationCount++;
    if (oledType == OLEDType::NONE) {
        return false;
    }
    if (oledForcedOff) return false;

    bool retVal = false;
    int lastNSize = networks.size();
    if ((_iterationCount % 30) == 0 || networks.size() <= 1) {
        //every 30 seconds, rescan network for new connections
        fillInNetworks();
        //if networks aren't configured or have changed, keep the oled on
        if (lastNSize != networks.size() || networks.size() <= 1) {
            retVal = true;
            displayOn = true;
        }
    }
    clearDisplay();
    
    Json::Value result;
    bool statusValid = getCurrentStatus(result);
    if (statusValid) {
        if (!displayOn || _lastStatusLines.empty()) {
            displayOn = checkIfStatusChanged(result);
            if (displayOn) {
                retVal = true;
            }
        }
    }
    
    if (displayOn) {
        int startY = 0;
        setTextSize(1);
        setTextColor(WHITE);
        if (oledType != OLEDType::TWO_COLOR || !oledFlipped) {
            startY = outputTopPart(startY, _iterationCount);
            if (oledType != OLEDType::TWO_COLOR) {
                // two color display doesn't need the separator line
                drawLine(0, startY, 127, startY, WHITE);
                startY++;
                if (LED_DISPLAY_HEIGHT > 65) {
                    startY += 2;
                }
            }
            startY = outputBottomPart(startY, _iterationCount, statusValid, result);
        } else {
            //strange case... with 2 color display flipped, we still need to keep the
            //network part in the "yellow" which is now below the main part
            outputBottomPart(0, _iterationCount, statusValid, result);
            outputTopPart(48, _iterationCount);
        }
    }
    Display();
    return retVal;
}

void FPPStatusOLEDPage::fillInNetworks() {
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

void FPPStatusOLEDPage::runTest(const std::string &test) {
    Json::Value val;
    val["cycleMS"] = 1000;
    val["enabled"] = 1;
    val["channelSet"] = "1-1048576";
    val["channelSetType"] = "channelRange";

    if (test == "R-G-B Chase") {
        val["mode"] = "RGBChase";
        val["subMode"] = "RGBChase-RGB";
        val["colorPattern"] = "FF000000FF000000FF";
    } else if (test == "R-G-B-W Chase") {
        val["mode"] = "RGBChase";
        val["subMode"] = "RGBChase-RGBA";
        val["colorPattern"] = "FF000000FF000000FFFFFFFF";
    } else if (test == "R-G-B-W-N Chase") {
        val["mode"] = "RGBChase";
        val["subMode"] = "RGBChase-RGBAN";
        val["colorPattern"] = "FF000000FF000000FFFFFFFF000000";
    } else if (test == "R-G-B Cycle") {
        val["mode"] = "RGBCycle";
        val["subMode"] = "RGBCycle-RGB";
        val["colorPattern"] = "FF000000FF000000FF";
    } else if (test == "R-G-B-W Cycle") {
        val["mode"] = "RGBCycle";
        val["subMode"] = "RGBCycle-RGBA";
        val["colorPattern"] = "FF000000FF000000FFFFFFFF";
    } else if (test == "R-G-B-W-N Cycle") {
        val["mode"] = "RGBCycle";
        val["subMode"] = "RGBCycle-RGBAN";
        val["colorPattern"] = "FF000000FF000000FFFFFFFF000000";
    } else if (test == "Off") {
        val["mode"] = "SingleChase";
        val["chaseSize"] = 3;
        val["chaseValue"] = 255;
        val["enabled"] = 0;
    } else if (test == "White") {
        val["mode"] = "RGBFill";
        val["color1"] = 255;
        val["color2"] = 255;
        val["color3"] = 255;
    } else if (test == "Red") {
        val["mode"] = "RGBFill";
        val["color1"] = 255;
        val["color2"] = 0;
        val["color3"] = 0;
    } else if (test == "Green") {
        val["mode"] = "RGBFill";
        val["color1"] = 0;
        val["color2"] = 255;
        val["color3"] = 0;
    } else if (test == "Blue") {
        val["mode"] = "RGBFill";
        val["color1"] = 0;
        val["color2"] = 0;
        val["color3"] = 255;
    } else {
        printf("Unknown test  %s\n", test.c_str());
        return;
    }
    
    std::string data = SaveJsonToString(val);
    
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

void FPPStatusOLEDPage::cycleTest() {
    _currentTest++;
    if (_currentTest == 1) {
        runTest("R-G-B Cycle");
    } else if (_currentTest == 2) {
        runTest("R-G-B-W-N Cycle");
    } else if (_currentTest == 3) {
        runTest("Off");
        _currentTest = 0;
    } else {
        _currentTest = 0;
    }
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
void FPPStatusOLEDPage::readImage() {
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
                        _imageWidth = std::atoi(v[2].c_str());
                    } else if (line.find("_height") != std::string::npos) {
                        std::vector<std::string> v = splitAndTrim(line, ' ');
                        _imageHeight = std::atoi(v[2].c_str());
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


bool FPPStatusOLEDPage::doAction(const std::string &action) {
    //printf("In do action  %s\n", action.c_str());
    if (action == "Test"
        || action == "Test/Down") {
        cycleTest();
        _curPage = 0;
    } else if (action == "Enter" && !oledForcedOff) {
        if (_hasSensors) {
            _curPage++;
            if (_curPage == MAX_PAGE) {
                _curPage = 0;
                SetCurrentPage(mainMenu);
            }
        } else {
            SetCurrentPage(mainMenu);
        }
    }
    return false;
}
