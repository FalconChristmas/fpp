#ifndef  __FPPOLEDUTILS__
#define  __FPPOLEDUTILS__

#include <vector>
#include <string>

#include <curl/curl.h>

class FPPOLEDUtils {
public:
    FPPOLEDUtils(int ledType);
    ~FPPOLEDUtils();

    bool doIteration(int count);
  
    
    void run();
    void cycleTest();
private:
    void fillInNetworks();
    int getSignalStrength(char *iwname);
    void outputNetwork(int idx, int y);
    
    int outputTopPart(int startY, int count);
    int outputBottomPart(int startY, int count);

    
    int _ledType;
    std::vector<std::string> networks;
    std::vector<int> signalStrength;
    
    int _currentTest;
    int _curPage;
    bool _displayOn;
    
    CURL *curl;
    std::string buffer;
    int sockfd;
    
    
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
};

#endif
