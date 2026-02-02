#include <sys/stat.h>
#include <thread>

#include "Ball.h"

static std::map<std::string, Ball> BALLS;
static std::map<uint32_t, uint8_t*> DOMAINS;

Ball& Ball::addBall(const std::string& n, uint32_t d, uint32_t o) {
    return BALLS[n].set(n, d, o);
}
Ball& Ball::findBall(const std::string& n) {
    return BALLS[n];
}
void Ball::setDomainAddress(uint32_t d, uint8_t* ad) {
    DOMAINS[d] = ad;
}

Ball& Ball::query() {
    if (offset) {
        uint8_t* d = DOMAINS[domain];
        d += offset;
        uint32_t* v = (uint32_t*)d;
        printf("\t%s:  %X", name.c_str(), *v);
        for (auto& m : modesetCommands) {
            if (!m.second.empty() && m.second.back() == *v) {
                printf("    %s", m.first.c_str());
            }
        }
        printf("\n");
    }
    return *this;
}
uint32_t Ball::queryRawMode() {
    if (offset) {
        uint8_t* d = DOMAINS[domain];
        d += offset;
        uint32_t* v = (uint32_t*)d;
        return *v;
    }
    return 0xFFFFFFFF;
}

std::string Ball::queryMode() {
    if (offset) {
        uint8_t* d = DOMAINS[domain];
        d += offset;
        uint32_t* v = (uint32_t*)d;
        for (auto& m : modesetCommands) {
            if (!m.second.empty() && m.second.back() == *v && m.first != "reset") {
                return m.first;
            }
        }
        for (auto& m : modesetCommands) {
            // allow returning reset if it is the only mode
            if (!m.second.empty() && m.second.back() == *v) {
                return m.first;
            }
        }
        /*
        for (auto& m : modesetCommands) {
            printf("   %s:  %X (%d)", m.first.c_str(), m.second.back(), m.second.size());
        }
        */
    }
    return "";
}

inline int FileExists(const char* File) {
    struct stat sts;
    if (stat(File, &sts) == -1) {
        return 0;
    } else {
        return 1;
    }
}

Ball& Ball::setMode(const std::string& m) {
    auto& c = modesetCommands[m];
    if (!c.empty()) {
        std::string pinMuxFile = "/sys/devices/platform/ocp/ocp:" + name + "_pinmux/state";
        if (FileExists(pinMuxFile.c_str())) {
            FILE* fd = fopen(pinMuxFile.c_str(), "w");
            if (fd != nullptr) {
                if (mappedModes.contains(m)) {
                    std::string m2 = mappedModes[m];
                    fwrite(m2.c_str(), m2.size(), 1, fd);
                } else {
                    fwrite(m.c_str(), m.size(), 1, fd);
                }
                fclose(fd);
            }
        }

        uint8_t* d = DOMAINS[domain];
        d += offset;
        uint32_t* v = (uint32_t*)d;

        uint32_t curVal = *(volatile uint32_t*)v;
        uint32_t set = 0;
        for (auto& cmd : c) {
            *(volatile uint32_t*)v = cmd;
            set = cmd;
            std::this_thread::yield();
        }
        uint32_t newVal = *(volatile uint32_t*)v;
        // printf("\t%s:  %X -> %X -> %X (%s)\n", name.c_str(), curVal, set, newVal, m.c_str());
    }
    return *this;
}
