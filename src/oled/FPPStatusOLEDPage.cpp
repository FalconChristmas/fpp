#include "fpp-pch.h"

#include <linux/wireless.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <fstream>
#include <ifaddrs.h>
#include <netdb.h>
#include <unistd.h>

#include "../common.h"

#include "FPPMainMenu.h"
#include "FPPStatusOLEDPage.h"

static inline int iw_get_ext(
    int skfd,           /* Socket to the kernel */
    const char* ifname, /* Device name */
    int request,        /* WE ID */
    struct iwreq* pwrq) /* Fixed part of the request */
{
    strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
    return (ioctl(skfd, request, pwrq));
}

#define MAX_PAGE 2

int FPPStatusOLEDPage::getSignalStrength(char* iwname) {
    int sigLevel = 0;

    iw_statistics stats;
    int has_stats;
    iw_range range;
    int has_range;

    struct iwreq wrq;
    char buffer[std::max(sizeof(iw_range), sizeof(iw_statistics)) * 2]; /* Large enough */
    iw_range* range_raw;

    /* Cleanup */
    bzero(buffer, sizeof(buffer));

    wrq.u.data.pointer = (caddr_t)buffer;
    wrq.u.data.length = sizeof(buffer);
    wrq.u.data.flags = 0;
    if (iw_get_ext(sockfd, iwname, SIOCGIWRANGE, &wrq) < 0) {
        // no ranges
        return sigLevel;
    }
    range_raw = (iw_range*)buffer;
    memcpy((char*)&range, buffer, sizeof(iw_range));
    if (range.max_qual.qual == 0) {
        return sigLevel;
    }
    wrq.u.data.pointer = (caddr_t)&stats;
    wrq.u.data.length = sizeof(struct iw_statistics);
    wrq.u.data.flags = 1;
    strncpy(wrq.ifr_name, iwname, IFNAMSIZ);
    if (iw_get_ext(sockfd, iwname, SIOCGIWSTATS, &wrq) < 0) {
        // no stats
        return sigLevel;
    }

    sigLevel = stats.qual.qual;
    sigLevel *= 100;
    sigLevel /= range.max_qual.qual;
    return sigLevel;
}

static bool debug = false;

static int writer(char* data, size_t size, size_t nmemb,
                  std::string* writerData) {
    if (writerData == NULL)
        return 0;
    writerData->append(data, size * nmemb);
    return size * nmemb;
}

FPPStatusOLEDPage::FPPStatusOLEDPage() :
    _currentTest(""),
    _testSpeed(500),
    _curPage(0),
    _doFullStatus(true),
    _iterationCount(0),
    _hasSensors(false) {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:32322/fppd/status");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    // have to use a socket for ioctl
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    readCapeImage();
    mainMenu = new FPPMainMenu(this);
}
FPPStatusOLEDPage::~FPPStatusOLEDPage() {
    curl_easy_cleanup(curl);
    close(sockfd);
    delete mainMenu;
}

void FPPStatusOLEDPage::outputNetwork(int idx, int y) {
    printString(0, y, networks[idx]);
    if (signalStrength[idx]) {
        y = y + 7;
        int cur = 5;
        for (int x = 0; x < 7; x++) {
            if (signalStrength[idx] > cur) {
                drawLine(127 - (x / 2), y, 128, y);
            }
            y--;
            cur += 15;
        }
    }
}

static bool compareLines(int x, std::vector<std::string>& lines1, std::vector<std::string>& lines2) {
    if (x < lines1.size() && x < lines2.size()) {
        return lines1[x] != lines2[x];
    } else if (x < lines1.size() || x < lines2.size()) {
        return true;
    }
    return false;
}

bool FPPStatusOLEDPage::checkIfStatusChanged(Json::Value& result) {
    std::string mode = result["mode_name"].asString();
    mode[0] = toupper(mode[0]);
    if (currentMode != mode) {
        return true;
    }

    // if the status has materially changed, turn the oled back on
    std::vector<std::string> lines;
    int maxLines = getLinesPage0(lines, result, true);
    if (compareLines(0, lines, _lastStatusLines)) {
        // main status
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(1, lines, _lastStatusLines)) {
        // running sequence
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(2, lines, _lastStatusLines)) {
        // running song
        _lastStatusLines = lines;
        return true;
    }
    if (compareLines(5, lines, _lastStatusLines)) {
        // running playlist
        _lastStatusLines = lines;
        return true;
    }

    return false;
}

int FPPStatusOLEDPage::getLinesPage0(std::vector<std::string>& lines,
                                     Json::Value& result,
                                     bool allowBlank) {
    std::string status = result["status_name"].asString();
    currentMode = result["mode_name"].asString();
    currentMode[0] = toupper(currentMode[0]);
    std::string line = currentMode;
    bool isIdle = (status == "idle" || status == "testing");
    int maxLines = 6;
    if (currentMode != "Bridge") {
        // bridge is always "idle" which isn't really true
        line += ": " + status;
    } else {
        // bridge mode doesn't output a playlist section
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
int FPPStatusOLEDPage::getLinesPage1(std::vector<std::string>& lines,
                                     Json::Value& result,
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
            snprintf(buf, sizeof(buf), "%.1f", i);
            line += buf;
            line += "F)";
        } else {
            line = result["sensors"][x]["label"].asString() + " " + result["sensors"][x]["formatted"].asString();
        }
        lines.push_back(line);
    }
    if (lines.empty()) {
        return getLinesPage0(lines, result, allowBlank);
    }
    return 6;
}
int FPPStatusOLEDPage::outputTopPart(int startY, int count) {
    if (networks.size() > 1) {
        int idx = (count / 3) % networks.size();
        int lines = GetLEDDisplayHeight() >= 64 ? 2 : 1;
        if (networks.size() <= 5 && GetLEDDisplayHeight() > 65) {
            idx = 0;
            lines = networks.size() < 5 ? networks.size() : 5;
            if (lines < 2)
                lines = 2;
        }
        if (networks.size() <= lines) {
            idx = 0;
        }
        outputNetwork(idx, startY);
        startY += 8;
        if (GetLEDDisplayHeight() > 65) {
            startY += 2; // little extra space
        }
        for (int x = 1; x < lines; x++) {
            idx++;
            if (idx >= networks.size()) {
                idx = 0;
            }
            outputNetwork(idx, startY);
            startY += 8;
            if (GetLEDDisplayHeight() > 65) {
                startY += 2; // little extra space
            }
        }
    } else {
        if (count < 60) {
            printString(0, startY, "FPP Booting...");
        } else {
            printString(0, startY, "No Network");
        }
        startY += 8;
        if (GetLEDDisplayHeight() >= 64) {
            startY += 8;
        }
    }
    return startY;
}
bool FPPStatusOLEDPage::getCurrentStatus(Json::Value& result) {
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

int FPPStatusOLEDPage::outputBottomPart(int startY, int count, bool statusValid, Json::Value& result) {
    if (statusValid) {
        std::vector<std::string> lines;
        std::string line;
        int maxLines = 5;
        if (_curPage == 0) {
            maxLines = getLinesPage0(lines, result, GetLEDDisplayHeight() >= 64);
        } else {
            maxLines = getLinesPage1(lines, result, GetLEDDisplayHeight() >= 64);
        }
        if (maxLines > lines.size()) {
            maxLines = lines.size();
        }
        if (GetLEDDisplayHeight() >= 64) {
            for (int x = 0; x < maxLines; x++) {
                line = lines[x];
                if (line.length() > 21) {
                    line.resize(21);
                }
                printString(0, startY, line);
                startY += 8;
                if (GetLEDDisplayHeight() > 65) {
                    startY += 2;
                }
            }
        } else {
            int numLines = (GetLEDDisplayHeight() - startY) / 8;
            int idx = count % maxLines;
            if (numLines >= maxLines) {
                idx = 0;
            }
            line = lines[idx];
            if (line.length() > 21) {
                line.resize(21);
            }
            printString(0, startY, line);
            startY += 8;
            idx++;
            if (idx == maxLines) {
                idx = 0;
            }
            if (maxLines > 1) {
                line = lines[idx];
                if (line.length() > 21) {
                    line.resize(21);
                }
                printString(0, startY, line);
            }
        }
    } else {
        if (count < 75) {
            // if less than 75 seconds since start, we'll assume we are booting up
            std::string line = "Booting.";
            int idx = count % 8;
            for (int i = 0; i < idx; i++) {
                line += ".";
            }
            printString(10, startY, line);
            startY += 8;
        } else {
            printString(0, startY, "FPPD is not running..");
            startY += 8;
        }
        if (_capeImageWidth && oledType != OLEDType::TEXT_ONLY) {
            int y = startY;
            if (oledType != OLEDType::TWO_COLOR) {
                --y;
            }
            if (GetLEDDisplayHeight() > 65) {
                y += 4;
            }

            drawBitmap(0, y, &_capeImage[0], _capeImageWidth, _capeImageHeight);
        }
    }
    return startY;
}

bool FPPStatusOLEDPage::doIteration(bool& displayOn) {
    _iterationCount++;
    if (oledType == OLEDType::NONE) {
        return false;
    }
    if (oledForcedOff)
        return false;

    bool retVal = false;
    int lastNSize = networks.size();
    if ((_iterationCount % 30) == 0 || networks.size() <= 1) {
        // every 30 seconds, rescan network for new connections
        fillInNetworks();
        // if networks aren't configured or have changed, keep the oled on
        if (lastNSize != networks.size() || networks.size() <= 1) {
            retVal = true;
            displayOn = true;
        }
    }

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
        clearDisplay();
        int startY = 0;
        int count = _iterationCount;
        bool doWiFi = false;
        if (count > 15 && loadWiFiImage()) {
            count -= 15;
            count %= 30;
            doWiFi = count < 15;
        }
        if (doWiFi) {
            displayWiFiQR();
        } else if (oledType != OLEDType::TWO_COLOR || !oledFlipped) {
            startY = outputTopPart(startY, _iterationCount);
            if (oledType != OLEDType::TWO_COLOR && oledType != OLEDType::TEXT_ONLY) {
                // two color display doesn't need the separator line
                drawLine(0, startY, 127, startY);
                startY++;
                if (GetLEDDisplayHeight() > 65) {
                    startY += 2;
                }
            }
            startY = outputBottomPart(startY, _iterationCount, statusValid, result);
        } else {
            // strange case... with 2 color display flipped, we still need to keep the
            // network part in the "yellow" which is now below the main part
            outputBottomPart(0, _iterationCount, statusValid, result);
            outputTopPart(48, _iterationCount);
        }
        flushDisplay();
    }
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

    // get all the addresses
    struct ifaddrs *interfaces, *tmp;
    getifaddrs(&interfaces);
    tmp = interfaces;
    char addressBuf[128];
    while (tmp) {
        int max = OLEDPage::GetLEDDisplayWidth() / 6;
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            if (strncmp("usb", tmp->ifa_name, 3) != 0) {
                // skip the usb* interfaces as we won't support multisync on those
                GetInterfaceAddress(tmp->ifa_name, addressBuf, NULL, NULL);
                if (strcmp(addressBuf, "127.0.0.1")) {
                    hn = tmp->ifa_name;
                    hn += ":";
                    hn += addressBuf;
                    if (hn.size() > max) {
                        hn = addressBuf;
                    }
                    networks.push_back(hn);
                    int i = getSignalStrength(tmp->ifa_name);
                    signalStrength.push_back(i);
                }
            }
        } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            // FIXME for ipv6 multisync
        } else {
            // printf("Not a netork: %s\n", tmp->ifa_name);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);
}

void FPPStatusOLEDPage::runTest(const std::string& test, bool ms) {
    printf("Running Test: %s    Speed: %dms\n", test.c_str(), _testSpeed);

    Json::Value val;
    if (test == "Off") {
        val["command"] = "Test Stop";
        val["args"] = Json::Value(Json::ValueType::arrayValue);
        _currentTest = "";
    } else {
        val["command"] = "Test Start";
        val["args"] = Json::Value(Json::ValueType::arrayValue);
        val["args"].append(std::to_string(_testSpeed));
        _currentTest = test;
    }
    _multisyncTest = ms;
    val["multisyncCommand"] = _multisyncTest;
    val["multisyncHosts"] = "";
    std::string channelRange = "";

    if (test == "R-G-B Chase") {
        val["args"].append("RGB Chase");
        val["args"].append(channelRange);
        val["args"].append("R-G-B");
    } else if (test == "R-G-B-W Chase") {
        val["args"].append("RGB Chase");
        val["args"].append(channelRange);
        val["args"].append("R-G-B-All");
    } else if (test == "R-G-B-W-N Chase") {
        val["args"].append("RGB Chase");
        val["args"].append(channelRange);
        val["args"].append("R-G-B-All-None");
    } else if (test == "R-G-B Cycle") {
        val["args"].append("RGB Cycle");
        val["args"].append(channelRange);
        val["args"].append("R-G-B");
    } else if (test == "R-G-B-W Cycle") {
        val["args"].append("RGB Cycle");
        val["args"].append(channelRange);
        val["args"].append("R-G-B-All");
    } else if (test == "R-G-B-W-N Cycle") {
        val["args"].append("RGB Cycle");
        val["args"].append(channelRange);
        val["args"].append("R-G-B-All-None");
    } else if (test == "White") {
        val["args"].append("RGB Single Color");
        val["args"].append(channelRange);
        val["args"].append("#ffffff");
    } else if (test == "Red") {
        val["args"].append("RGB Single Color");
        val["args"].append(channelRange);
        val["args"].append("#ff0000");
    } else if (test == "Green") {
        val["args"].append("RGB Single Color");
        val["args"].append(channelRange);
        val["args"].append("#00ff00");
    } else if (test == "Blue") {
        val["args"].append("RGB Single Color");
        val["args"].append(channelRange);
        val["args"].append("#0000ff");
    } else if (_currentTest != "") {
        printf("Unknown test %s\n", test.c_str());
        return;
    }

    std::string data = SaveJsonToString(val);

    buffer.clear();
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/api/command");

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
static std::vector<std::string> TESTS = { "Off", "R-G-B Cycle", "R-G-B-W-N Cycle", "R-G-B Chase", "R-G-B-W-N Chase" };
void FPPStatusOLEDPage::cycleTest() {
    int idx = 1;
    for (int x = 0; x < TESTS.size(); x++) {
        if (TESTS[x] == _currentTest) {
            idx = x + 1;
            break;
        }
    }
    if (idx >= TESTS.size()) {
        idx = 0;
    }
    _currentTest = TESTS[idx];
    runTest(_currentTest, _multisyncTest);
}

void FPPStatusOLEDPage::displayWiFiQR() {
    clearDisplay();
    int dh = GetLEDDisplayHeight();
    int y = 0;
    if (oledType == OLEDType::TWO_COLOR) {
        if (GetOLEDOrientationFlipped()) {
            y = 0;
        } else if (_wifiImageHeight > 48) {
            y = 15;
        } else {
            y = 16;
        }
    } else {
        if (_wifiImageHeight > dh) {
            y = 0;
        } else {
            y = (dh - _wifiImageHeight) / 2;
        }
    }
    drawBitmap(2, y, &_wifiImage[0], _wifiImageWidth, _wifiImageHeight);
    printString(12 + _wifiImageWidth, y + 12, "WiFi");
    printString(12 + _wifiImageWidth, y + 20, "Hot Spot");
}

bool FPPStatusOLEDPage::loadWiFiImage(const std::string& tp) {
    std::string fn = "/home/fpp/media/tmp/wifi-" + tp + ".ascii";
    if (_wifiImageWidth) {
        if (!FileExists(fn)) {
            // file no longer exists, clear
            _wifiImageWidth = 0;
            _wifiImageHeight = 0;
            _wifiImage.clear();
            return false;
        }
        return true;
    }
    _wifiImageWidth = 0;
    _wifiImageHeight = 0;
    int maxSz = 0;
    if (GetOLEDType() == OLEDPage::OLEDType::SMALL) {
        maxSz = 15;
    } else if (GetOLEDType() == OLEDPage::OLEDType::SINGLE_COLOR) {
        maxSz = 66;
    } else if (GetOLEDType() == OLEDPage::OLEDType::TWO_COLOR) {
        maxSz = 50;
    }
    if (maxSz && FileExists(fn)) {
        std::ifstream file(fn);
        if (file.is_open()) {
            std::string line;
            bool readingBytes = false;
            int scale = 2;
            while (std::getline(file, line)) {
                if (_wifiImageWidth == 0) {
                    if (line.size() > maxSz) {
                        // going to have to scale, first check if we can get the "H" version
                        if (tp == "L" && loadWiFiImage("H")) {
                            return true;
                        }
                        _wifiImageWidth = (int)line.size() / 2;
                        scale = 1;
                    } else {
                        _wifiImageWidth = (int)line.size();
                    }
                }
                for (int lc = 0; lc < scale; lc++) {
                    for (int x = 0; x < (_wifiImageWidth / scale);) {
                        uint8_t v = 0;
                        for (int y = 0; y < 8 / scale; y++) {
                            int idx = x * 2 + y * 2;
                            v <<= 1;
                            if (idx < line.size() && line[idx] == '#') {
                                v |= 1;
                                if (scale == 2) {
                                    v <<= 1;
                                    v |= 1;
                                }
                            } else if (scale == 2) {
                                v <<= 1;
                            }
                        }
                        x += 8 / scale;
                        _wifiImage.push_back(v);
                    }
                    ++_wifiImageHeight;
                }
            }
            file.close();
        }
        return true;
    }
    return false;
}

bool FPPStatusOLEDPage::doAction(const std::string& action) {
    // printf("In do action  %s   %d   %s\n", action.c_str(), _testSpeed, _currentTest.c_str());
    if (action == "Test" || action == "Test/Down" || action == "Test Multisync") {
        _multisyncTest = action == "Test Multisync";
        _testSpeed = 500;
        cycleTest();
        _curPage = 0;
    } else if (action == "Up" && _currentTest != "") {
        _testSpeed *= 2;
        runTest(_currentTest, _multisyncTest);
    } else if (action == "Down" && _currentTest != "") {
        _testSpeed /= 2;
        if (_testSpeed < 100) {
            _testSpeed = 100;
        }
        runTest(_currentTest, _multisyncTest);
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
