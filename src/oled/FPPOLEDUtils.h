#ifndef  __FPPOLEDUTILS__
#define  __FPPOLEDUTILS__

#include <vector>
#include <string>

#include <curl/curl.h>

class FPPOLEDUtils {
public:
    FPPOLEDUtils(int ledType);
    ~FPPOLEDUtils();

    void doIteration(int count);
  
    
    void run();
    void cycleTest();
private:
    void fillInNetworks();
    int getSignalStrength(char *iwname);
    void outputNetwork(int idx, int y);
    
    int _ledType;
    std::vector<std::string> networks;
    std::vector<int> signalStrength;
    
    int _currentTest;
    
    CURL *curl;
    std::string buffer;
    int sockfd;
    
    
    class InputAction {
    public:
        std::string pin;
        std::string mode;
        std::string edge;
        std::string action;
        int actionValue;
        int file;
        
        long long lastActionTime;
    };
    void parseInputActions(const std::string &file, std::vector<InputAction> &actions);
};

#endif
