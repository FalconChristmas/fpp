#pragma once

#include <list>
#include <map>
#include <string>

class Pin {
public:
    Pin() = default;

    Pin(const std::string& n) :
        name(n) {}

    std::string name;

    Pin& addBall(const std::string& n) {
        balls.emplace_back(n);
        return *this;
    }
    Pin& addMode(const std::string& m, const std::string& b, const std::string& bm) {
        modes[m] = { b, bm };
        return *this;
    }
    Pin& modesFromBall(const std::string& b = "");
    Pin& modesFromBallNoConflicts(const std::string& b = "");

    const Pin& query() const;
    const Pin& listModes() const;
    const Pin& setMode(const std::string& m) const;

    std::list<std::string> balls;
    std::map<std::string, std::pair<std::string, std::string>> modes;

    static Pin& addPin(const std::string& n);
    static Pin& getPin(const std::string& n);
    static const std::map<std::string, Pin>& getAllPins();
};
