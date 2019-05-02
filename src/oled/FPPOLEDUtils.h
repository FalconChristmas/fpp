#ifndef  __FPPOLEDUTILS__
#define  __FPPOLEDUTILS__

#include <vector>
#include <array>
#include <string>

class FPPStatusOLEDPage;

extern "C" {
    struct gpiod_chip;
    struct gpiod_line;
}

class FPPOLEDUtils {
public:
    FPPOLEDUtils(int ledType);
    ~FPPOLEDUtils();
    
    void run();
    void cleanup();

private:
    std::array<gpiod_chip*, 5> gpiodChips;

    int _ledType;
    bool _displayOn;
    FPPStatusOLEDPage *statusPage;
    class InputAction {
    public:
        class Action {
        public:
            Action(const std::string &a, int min, int max, long long mai)
                : action(a), actionValueMin(min), actionValueMax(max), minActionInterval(mai), nextActionTime(0) {}
            std::string action;
            int actionValueMin;
            int actionValueMax;
            long long minActionInterval;
            long long nextActionTime;
        };
        
        std::string pin;
        std::string mode;
        std::string edge;
        int file;
        int pollIndex;
        std::vector<Action> actions;
        
        const std::string &checkAction(int i, long long time);
        
        struct gpiod_line *gpiodLine;
    };
    
    std::vector<InputAction> actions;

    bool parseInputActions(const std::string &file);
    bool checkStatusAbility();
};

#endif
