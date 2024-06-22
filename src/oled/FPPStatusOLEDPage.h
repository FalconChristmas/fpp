#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2023 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */
#include "OLEDPages.h"
#include <curl/curl.h>
#include <jsoncpp/json/json.h>

class MenuOLEDPage;

class FPPStatusOLEDPage : public OLEDPage {
public:
    FPPStatusOLEDPage();
    virtual ~FPPStatusOLEDPage();

    virtual void displaying() override {
        bool on = true;
        _curPage = 0;
        doIteration(on);
    }
    virtual bool doIteration(bool& displayOn) override;
    virtual bool doAction(const std::string& action) override;

    int getLinesPage0(std::vector<std::string>& lines,
                      Json::Value& result,
                      bool allowBlank);
    int getLinesPage1(std::vector<std::string>& lines,
                      Json::Value& result,
                      bool allowBlank);
    void readImage();

    void fillInNetworks();
    int getSignalStrength(char* iwname);
    void outputNetwork(int idx, int y);

    int outputTopPart(int startY, int count);
    int outputBottomPart(int startY, int count, bool statusValid, Json::Value& result);

    void cycleTest();
    void runTest(const std::string& test, bool ms);

    void disableFullStatus() { _doFullStatus = false; }

    const std::string& getCurrentMode() const { return currentMode; }

    bool isMultiSyncTest() const { return _multisyncTest; }
    void setMultiSyncTest(bool b) { _multisyncTest = b; }

private:
    bool getCurrentStatus(Json::Value& result);
    bool checkIfStatusChanged(Json::Value& result);
    bool loadWiFiImage(const std::string& tp = "L");
    void displayWiFiQR();
    std::vector<std::string> _lastStatusLines;

    std::vector<std::string> networks;
    std::vector<int> signalStrength;

    int _iterationCount;
    std::string _currentTest;
    int _testSpeed;
    bool _multisyncTest = false;
    int _curPage;
    bool _doFullStatus;

    int _imageWidth;
    int _imageHeight;
    std::vector<uint8_t> _image;

    int _wifiImageWidth = 0;
    int _wifiImageHeight = 0;
    std::vector<uint8_t> _wifiImage;
    int wifiTimer = 0;

    CURL* curl;
    std::string buffer;
    int sockfd;

    bool _hasSensors;
    std::string currentMode;

    MenuOLEDPage* mainMenu;
};
