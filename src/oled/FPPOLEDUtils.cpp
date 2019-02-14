#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>
#include <strings.h>

#include "common.h"
#include "I2C.h"
#include "SSD1306_OLED.h"

#include "FPPOLEDUtils.h"

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

static int getSignalInfo(char *iwname){
    
    int sigLevel = 0;

    iw_statistics stats;
    int           has_stats;
    iw_range      range;
    int           has_range;

    //have to use a socket for ioctl
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    
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
    close(sockfd);
    return sigLevel;
}

static int writer(char *data, size_t size, size_t nmemb,
                  std::string *writerData)
{
    if(writerData == NULL)
        return 0;
    
    writerData->append(data, size*nmemb);
    return size * nmemb;
}
FPPOLEDUtils::FPPOLEDUtils(int ledType) : _ledType(ledType) {
    if (_ledType & 0x1) {
        SSD1306_LCDWIDTH = 128;
        SSD1306_LCDHEIGHT = 32;
    } else {
        SSD1306_LCDWIDTH = 128;
        SSD1306_LCDHEIGHT = 64;
    }
    
    if (_ledType & 0x2) {
        setRotation(2);
    } else {
        setRotation(0);
    }
    
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/api/fppd/status");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
}
FPPOLEDUtils::~FPPOLEDUtils() {
    curl_easy_cleanup(curl);
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

inline std::string getLine(int i,  Json::Value &result, bool nextIfBlank) {
    std::string line;
    switch (i) {
        case 0:
            line = result["current_sequence"].asString();
            if (line != "" || !nextIfBlank) {
                return line;
            }
        case 1:
            line = result["current_song"].asString();
            if (line != "" || !nextIfBlank) {
                return line;
            }
        case 2:
            line = result["time_elapsed"].asString();
            if (line != "") {
                line = "Elapsed: " + line;
                return line;
            }
            if (!nextIfBlank) {
                return line;
            }
        case 3:
            line = result["time_remaining"].asString();
            if (line != "") {
                line = "Remaining: " + line;
                return line;
            }
            if (!nextIfBlank) {
                return line;
            }
        case 4:
            line = result["current_playlist"]["playlist"].asString();
            break;
    };
    return line;
}

void FPPOLEDUtils::doIteration(int count) {
    networks.clear();
    if ((count % 30) == 0 || networks.empty()) {
        //every 30 seconds, rescan network for new connections
        fillInNetworks();
    }
    clearDisplay();
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
            setCursor(0, 17);

            std::string status = result["status_name"].asString();
            std::string mode = result["mode_name"].asString();
            mode[0] = toupper(mode[0]);
            std::string line = mode;
            bool isIdle = (status == "idle");
            int maxLines = 5;
            if (mode != "bridge") {
                //bridge is always "idle" which isn't really true
                line += ": " + status;
            } else {
                //bridge mode doesn't output a playlist section
                isIdle = false;
                maxLines = 4;
            }
            
            if (line.length() > 21) {
                line.resize(21);
            }
            print_str(line.c_str());
            
            if (!isIdle) {
                if (SSD1306_LCDHEIGHT == 64) {
                    for (int x = 0; x < maxLines; x++) {
                        setCursor(0, 25 + x * 8);
                        line = getLine(x, result, false);
                        if (line.length() > 21) {
                            line.resize(21);
                        }
                        print_str(line.c_str());
                    }
                } else {
                    setCursor(0, 25);
                    line = getLine(count % maxLines, result, true);
                    if (line.length() > 21) {
                        line.resize(21);
                    }
                    print_str(line.c_str());
                }
            }
        }
    }
    if (!gotStatus) {
        setTextSize(1);
        setTextColor(WHITE);
        setCursor(0, 18);
        print_str("FPPD is not");
        setCursor(0, 26);
        print_str("running ...");
    }
    
    setTextSize(1);
    setTextColor(WHITE);
    setCursor(0,0);
    if (networks.size() > 1) {
        int idx = count / 2;
        idx %= networks.size();
        if (networks.size() == 2) {
            idx = 0;
        }
        outputNetwork(idx, 0);
        idx++;
        if (idx >= networks.size()) {
            idx = 0;
        }
        outputNetwork(idx, 8);
    } else {
        print_str("No Network");
    }
    drawLine(0, 16, 127, 16, WHITE);
    
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
                    int i = getSignalInfo(tmp->ifa_name);
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


