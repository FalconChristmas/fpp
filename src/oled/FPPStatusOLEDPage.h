#ifndef  __FPPSTATUSOLEDPAGE__
#define  __FPPSTATUSOLEDPAGE__

#include "OLEDPages.h"
#include <jsoncpp/json/json.h>
#include <curl/curl.h>

class MenuOLEDPage;

class FPPStatusOLEDPage : public OLEDPage {
public:
    FPPStatusOLEDPage();
    virtual ~FPPStatusOLEDPage();
    
    virtual void displaying() override {
        bool on = true;
        doIteration(on);
    }
    virtual bool doIteration(bool &displayOn) override;
    virtual bool doAction(const std::string &action) override;
    
    int getLinesPage0(std::vector<std::string> &lines,
                      Json::Value &result,
                      bool allowBlank);
    int getLinesPage1(std::vector<std::string> &lines,
                      Json::Value &result,
                      bool allowBlank);
    void readImage();
    
    
    void fillInNetworks();
    int getSignalStrength(char *iwname);
    void outputNetwork(int idx, int y);
    
    int outputTopPart(int startY, int count);
    int outputBottomPart(int startY, int count);
    
    void cycleTest();
    void runTest(const std::string &test);
    
    void disableFullStatus() { _doFullStatus = false; }
    
    const std::string &getCurrentMode() const { return currentMode; }
private:
    std::vector<std::string> networks;
    std::vector<int> signalStrength;
    
    int _iterationCount;
    int _currentTest;
    int _curPage;
    bool _doFullStatus;
    
    int _imageWidth;
    int _imageHeight;
    std::vector<uint8_t> _image;
    
    CURL *curl;
    std::string buffer;
    int sockfd;
    
    bool _hasSensors;
    std::string currentMode;
    
    MenuOLEDPage *mainMenu;
};



#endif
