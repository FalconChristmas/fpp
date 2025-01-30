#include "Pin.h"
#include "Ball.h"

static std::map<std::string, Pin> PINS;

Pin& Pin::addPin(const std::string& n) {
    PINS.emplace(n, n);
    return PINS[n];
}
Pin& Pin::getPin(const std::string& n) {
    return PINS[n];
}

const std::map<std::string, Pin>& Pin::getAllPins() {
    return PINS;
}

const Pin& Pin::query() const {
    for (auto& b : balls) {
        Ball::findBall(b).query();
    }
    return *this;
}
const Pin& Pin::listModes() const {
    std::string qm = "";
    for (auto& b : balls) {
        std::string bm = Ball::findBall(b).queryMode();
        if (!bm.empty()) {
            qm = bm;
            break;
        }
    }
    bool found = false;
    for (const auto& md : modes) {
        if (md.first == qm) {
            printf("[%s] ", md.first.c_str());
            found = true;
        } else {
            printf("%s ", md.first.c_str());
        }
    }
    if (!found) {
        if (qm == "") {
            for (auto& b : balls) {
                uint32_t bm = Ball::findBall(b).queryRawMode();
                printf("     Cur Mode: %X", bm);
            }
        } else {
            printf("[%s] ", qm.c_str());
        }
    }
    printf("\n");
    return *this;
}

const Pin& Pin::setMode(const std::string& m) const {
    const auto& md = modes.find(m);
    if (md != modes.end()) {
        // first, reset the "other" balls so they go into input mode
        for (auto& b : balls) {
            if (b != md->second.first) {
                Ball::findBall(b).setMode("reset");
            }
        }
        // now set the target mode
        auto& b = Ball::findBall(md->second.first);
        b.setMode("reset");
        b.setMode(md->second.second);
    } else {
        printf("Unknown mode for pin %s:  %s\n", name.c_str(), m.c_str());
    }
    return *this;
}
Pin& Pin::modesFromBall(const std::string& b) {
    std::string bn = b.empty() ? balls.front() : b;
    auto& ball = Ball::findBall(bn);
    for (auto& m : ball.modesetCommands) {
        if (m.first != "reset") {
            addMode(m.first, bn, m.first);
        }
    }
    return *this;
}
Pin& Pin::modesFromBallNoConflicts(const std::string& b) {
    std::string bn = b.empty() ? balls.front() : b;
    auto& ball = Ball::findBall(bn);
    for (auto& m : ball.modesetCommands) {
        if (m.first != "reset" && modes.find(m.first) == modes.end()) {
            addMode(m.first, bn, m.first);
        }
    }
    return *this;
}
