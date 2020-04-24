
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "OLEDPages.h"
#include "FPPOLEDUtils.h"
#include "settings.h"
#include "common.h"

#include "util/BBBUtils.h"


static FPPOLEDUtils *oled = nullptr;
void sigInteruptHandler(int sig) {
    oled->cleanup();
    OLEDPage::displayOff();
    exit(1);
}

void sigTermHandler(int sig) {
    oled->cleanup();
    OLEDPage::displayOff();
    exit(0);
}


int main (int argc, char *argv[]) {
    PinCapabilities::InitGPIO("FPPOLED");
    printf("FPP OLED Status Display Driver\n");
    initSettings(argc, argv);
    if (DirectoryExists("/home/fpp")) {
        printf("    Loading Settings\n");
        loadSettings("/home/fpp/media/settings");
    }
    int ledType = getSettingInt("LEDDisplayType");
    printf("    Led Type: %d\n", ledType);
    fflush(stdout);
    
    if (!OLEDPage::InitializeDisplay(ledType)) {
        ledType = 0;
    }

    struct sigaction sigIntAction;
    sigIntAction.sa_handler = sigInteruptHandler;
    sigemptyset(&sigIntAction.sa_mask);
    sigIntAction.sa_flags = 0;
    sigaction(SIGINT, &sigIntAction, NULL);
    
    struct sigaction sigTermAction;
    sigTermAction.sa_handler = sigTermHandler;
    sigemptyset(&sigTermAction.sa_mask);
    sigTermAction.sa_flags = 0;
    sigaction(SIGTERM, &sigTermAction, NULL);
    
    int count = 0;
    oled = new FPPOLEDUtils(ledType);
    oled->run();
}
