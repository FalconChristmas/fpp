#ifndef  __FPPOLEDUTILS__
#define  __FPPOLEDUTILS__

#include <vector>
#include <string>

class FPPStatusOLEDPage;

class FPPOLEDUtils {
public:
    FPPOLEDUtils(int ledType);
    ~FPPOLEDUtils();
    
    void run();

private:
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
        std::vector<Action> actions;
        
        const std::string &checkAction(int i, long long time);
    };
    bool parseInputActions(const std::string &file, std::vector<InputAction> &actions);
    bool checkStatusAbility();
};

#endif
