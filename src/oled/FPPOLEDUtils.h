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
    
private:
    void fillInNetworks();
    int getSignalStrength(char *iwname);
    void outputNetwork(int idx, int y);
    
    int _ledType;
    std::vector<std::string> networks;
    std::vector<int> signalStrength;
    
    CURL *curl;
    std::string buffer;
    int sockfd;
};

#endif
