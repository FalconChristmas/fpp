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
    // Returns false if this pin has no such mode, so callers can tell a rejected
    // mode from an applied one.  A silent "success" here is dangerous: FPP's
    // CapeUtils::ConfigurePin() takes a zero exit as "the mux is set" and skips
    // its fallback, so a mode this tool doesn't know would leave the pad wherever
    // it happened to be.
    bool setMode(const std::string& m) const;

    std::list<std::string> balls;
    std::map<std::string, std::pair<std::string, std::string>> modes;

    static Pin& addPin(const std::string& n);
    static Pin& getPin(const std::string& n);
    // Unlike getPin(), does not print or insert a placeholder for a name this
    // board doesn't have.
    static bool hasPin(const std::string& n);
    static const std::map<std::string, Pin>& getAllPins();
};
