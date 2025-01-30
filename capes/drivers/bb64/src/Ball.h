#pragma once

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

    Ball& addMode(const std::string& n, const std::vector<uint32_t>& c) {
        modesetCommands.emplace(n, c);
        return *this;
    }

    std::string queryMode();
    uint32_t queryRawMode();
    Ball& query();
    Ball& setMode(const std::string& m);

    std::string name;
    uint32_t domain = 0;
    uint32_t offset = 0;

    std::map<std::string, std::vector<uint32_t>> modesetCommands;

    static Ball& addBall(const std::string& n, uint32_t d, uint32_t o);
    static Ball& findBall(const std::string& n);

    static void setDomainAddress(uint32_t d, uint8_t* ad);
};