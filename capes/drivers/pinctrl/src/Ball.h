#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Ball {
public:
    Ball() {}
    Ball(uint32_t d, uint32_t o) :
        domain(d), offset(o) {}
    Ball(const std::string& n, uint32_t d, uint32_t o) :
        name(n), domain(d), offset(o) {}

    Ball& set(const std::string& n, uint32_t d, uint32_t o) {
        name = n;
        domain = d;
        offset = o;
        return *this;
    }

    Ball& addMode(const std::string& n, uint32_t c) {
        modesetCommands.emplace(n, std::vector<uint32_t>{ c });
        return *this;
    }
    Ball& addMode(const std::string& n, const std::vector<uint32_t>& c) {
        modesetCommands.emplace(n, c);
        return *this;
    }
    Ball& addMode(const std::string& n, const std::string& mn, const std::vector<uint32_t>& c) {
        modesetCommands.emplace(n, c);
        mappedModes.emplace(n, mn);
        return *this;
    }
    Ball& addMode(const std::string& n, const std::string& mn, uint32_t c) {
        modesetCommands.emplace(n, std::vector<uint32_t>{ c });
        mappedModes.emplace(n, mn);
        return *this;
    }
    Ball& addMode(const std::string& n, const std::string& mn) {
        std::vector<uint32_t> c = modesetCommands[mn];
        modesetCommands.emplace(n, c);
        mappedModes.emplace(n, mn);
        return *this;
    }

    std::string queryMode();
    uint32_t queryRawMode();
    Ball& query();
    Ball& setMode(const std::string& m);

    std::string name;
    uint32_t domain = 0;
    uint32_t offset = 0;
    uint32_t modeMask = 0x7;

    std::map<std::string, std::vector<uint32_t>> modesetCommands;
    std::map<std::string, std::string> mappedModes;

    static Ball& addBall(const std::string& n, uint32_t d, uint32_t o);
    static Ball& findBall(const std::string& n);

    static void setDomainAddress(uint32_t d, uint8_t* ad);
};