#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "Ball.h"

static std::map<std::string, Ball> BALLS;

// A domain is accessed either by a raw mmap pointer (pointer-poke, used by the
// K3/AM62 path and the AM335x /dev/mem fallback) or, when /dev/am335xpm is
// present, through a file descriptor: the AM335x conf registers are only
// writable from kernel mode, so register access is mediated by the driver via
// pread/pwrite. fdBase is the ball offset that corresponds to fd offset 0.
struct DomainAccess {
    uint8_t* ptr = nullptr;
    int fd = -1;
    uint32_t fdBase = 0;
};
static std::map<uint32_t, DomainAccess> DOMAINS;

static uint32_t readReg(uint32_t domain, uint32_t offset) {
    auto& da = DOMAINS[domain];
    if (da.fd >= 0) {
        uint32_t v = 0xFFFFFFFF;
        if (pread(da.fd, &v, sizeof(v), offset - da.fdBase) != (ssize_t)sizeof(v)) {
            return 0xFFFFFFFF;
        }
        return v;
    }
    if (da.ptr) {
        return *(volatile uint32_t*)(da.ptr + offset);
    }
    return 0xFFFFFFFF;
}
static void writeReg(uint32_t domain, uint32_t offset, uint32_t val) {
    auto& da = DOMAINS[domain];
    if (da.fd >= 0) {
        // Kernel performs the privileged register write.
        [[maybe_unused]] ssize_t r = pwrite(da.fd, &val, sizeof(val), offset - da.fdBase);
    } else if (da.ptr) {
        *(volatile uint32_t*)(da.ptr + offset) = val;
    }
}
static bool domainUsesFd(uint32_t domain) {
    return DOMAINS[domain].fd >= 0;
}

Ball& Ball::addBall(const std::string& n, uint32_t d, uint32_t o) {
    return BALLS[n].set(n, d, o);
}
Ball& Ball::findBall(const std::string& n) {
    return BALLS[n];
}
void Ball::setDomainAddress(uint32_t d, uint8_t* ad) {
    DOMAINS[d].ptr = ad;
}
void Ball::setDomainFd(uint32_t d, int fd, uint32_t base) {
    DOMAINS[d].fd = fd;
    DOMAINS[d].fdBase = base;
}

Ball& Ball::query() {
    if (offset) {
        uint32_t v = readReg(domain, offset);
        printf("\t%s:  %X", name.c_str(), v);
        for (auto& m : modesetCommands) {
            if (!m.second.empty() && m.second.back() == v) {
                printf("    %s", m.first.c_str());
            }
        }
        printf("\n");
    }
    return *this;
}
uint32_t Ball::queryRawMode() {
    if (offset) {
        return readReg(domain, offset);
    }
    return 0xFFFFFFFF;
}

std::string Ball::queryMode() {
    if (offset) {
        uint32_t v = readReg(domain, offset);
        for (auto& m : modesetCommands) {
            if (!m.second.empty() && m.second.back() == v && m.first != "reset") {
                return m.first;
            }
        }
        for (auto& m : modesetCommands) {
            // allow returning reset if it is the only mode
            if (!m.second.empty() && m.second.back() == v) {
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
        if (!domainUsesFd(domain)) {
            // Legacy path: the AM335x conf registers are not writable from
            // userspace, so the actual mux change comes from the cape-universal
            // pinmux-helper. When /dev/am335xpm is present the kernel performs
            // the register write below instead, and this is skipped.
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
        }

        for (auto& cmd : c) {
            writeReg(domain, offset, cmd);
            std::this_thread::yield();
        }
        // uint32_t newVal = readReg(domain, offset);
        // printf("\t%s:  -> %X (%s)\n", name.c_str(), newVal, m.c_str());
    }
    return *this;
}
