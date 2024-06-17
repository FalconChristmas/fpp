
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#include "FPPOLEDUtils.h"
#include "OLEDPages.h"
#include "common.h"
#include "settings.h"

#if defined(PLATFORM_BBB)
#include "util/BBBUtils.h"
#define PLAT_GPIO_CLASS BBBPinProvider
#elif defined(PLATFORM_PI)
#include "util/PiGPIOUtils.h"
#define PLAT_GPIO_CLASS PiGPIOPinProvider
#else
#include "util/TmpFileGPIO.h"
#define PLAT_GPIO_CLASS TmpFilePinProvider
#endif
#include <thread>

static FPPOLEDUtils* oled = nullptr;
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

int main(int argc, char* argv[]) {
    printf("FPP OLED Status Display Driver\n");
    int lt = getRawSettingInt("LEDDisplayType", 7);
    if (!OLEDPage::InitializeDisplay(lt)) {
        lt = 0;
    } else {
        OLEDPage::displayBootingNotice();
    }
    bool capeDetectionDone = FileExists("/home/fpp/media/tmp/cape_detect_done");
    int count = 0;
    while (!capeDetectionDone && count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++count;
        capeDetectionDone = FileExists("/home/fpp/media/tmp/cape_detect_done");
    }

    // wait until after cape detection so gpio expanders are registered and available
    PinCapabilities::InitGPIO("FPPOLED", new PLAT_GPIO_CLASS());
    LoadSettings(argv[0]);
    int ledType = getRawSettingInt("LEDDisplayType", 99);
    printf("    Led Type: %d\n", ledType);
    fflush(stdout);
    if (lt != ledType) {
        if (ledType == 99) {
            ledType = lt;
        } else if (!OLEDPage::InitializeDisplay(ledType)) {
            ledType = 0;
        }
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

    oled = new FPPOLEDUtils(ledType);
    oled->run();
}
