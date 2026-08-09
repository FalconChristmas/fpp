
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fpp-json.h"

#include "FPPOLEDUtils.h"
#include "OLEDPages.h"
#include "common.h"
#include "log.h"
#include "settings.h"

#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
#include "util/BBBUtils.h"
#define PLAT_GPIO_CLASS BBBPinProvider
#elif defined(PLATFORM_PI)
#include "util/PiGPIOUtils.h"
#define PLAT_GPIO_CLASS PiGPIOPinProvider
#else
#include "util/TmpFileGPIO.h"
#define PLAT_GPIO_CLASS TmpFilePinProvider
#endif
#include <map>
#include <thread>

class WarningHolder {
    static void AddWarning(const std::string& w);
    // Mirror the real Warnings.h signature (incl. the defaulted data map) so
    // GPIOUtils.o — compiled against the real header — links against this stub.
    static void AddWarning(int id, const std::string& w, const std::map<std::string, std::string>& data = {});
};

void WarningHolder::AddWarning(const std::string& w) {
    LogWarn(VB_GENERAL, "Warning: %s\n", w.c_str());
}
void WarningHolder::AddWarning(int id, const std::string& w, const std::map<std::string, std::string>& data) {
    LogWarn(VB_GENERAL, "Warning: %s\n", w.c_str());
}

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
    // "logFile" isn't a persisted setting - fppd.cpp only ever sets it in its
    // own memory at startup, defaulting to FPP_FILE_LOG - so use that same
    // constant directly rather than a getSetting() lookup that would just
    // come back empty here. toStdOut=true keeps the journalctl-visible output
    // (fppoled has no daemonize/foreground flag to check like fppd does) while
    // also merging into the same shared fppd.log everything else writes to -
    // see log.cpp's note that fppd.log is a multi-writer log, each line tagged
    // with the real program name automatically. fppd asks for the stdout copy
    // to be dropped once a line is in the file (SetSuppressDuplicateStdOut);
    // fppoled deliberately does not - it is a handful of lines a boot, and
    // `journalctl -u fppoled` is where they are looked for.
    //
    // Everything below logs rather than printf()s for the same reason: a raw
    // printf reaches only the journal, which on these boxes is RAM-only and
    // never reaches a Support Zip.
    SetLogFile(FPP_FILE_LOG.c_str(), true);
    LogInfo(VB_GENERAL, "FPP OLED Status Display Driver\n");
    int lt = getRawSettingInt("LEDDisplayType", 7);
    LogInfo(VB_GENERAL, "    LED Display Type: %d\n", lt);
    if (!OLEDPage::InitializeDisplay(lt)) {
        lt = 0;
    }
    int count = 0;
    if (argc > 1) {
        if (lt != 0) {
            OLEDPage::displayBootingNotice("FPP - File Systems");
        }
        if (strcmp(argv[1], "clear") == 0) {
            unlink("/home/fpp/media/tmp/cape_detect_done");
            while (FileExists("/home/fpp/media/tmp/cape_detect_done") && count < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                unlink("/home/fpp/media/tmp/cape_detect_done");
                count++;
            }
            if (FileExists("/home/fpp/media/tmp/cape-image.xbm")) {
                CopyFileContents("/home/fpp/media/tmp/cape-image.xbm", "/var/tmp/cape-image.xbm");
            }
            return 0;
        }
    }
    if (lt != 0) {
        OLEDPage::displayBootingNotice("FPP - Cape Detection");
    }
    count = 0;
    bool capeDetectionDone = FileExists("/home/fpp/media/tmp/cape_detect_done");
    while (!capeDetectionDone && count < 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++count;
        capeDetectionDone = FileExists("/home/fpp/media/tmp/cape_detect_done");
    }

    // wait until after cape detection so gpio expanders are registered and available
    if (lt < 10 || lt > 15) {
        // The i2c1602 displays will already initialize GPIO internally
        PinCapabilities::InitGPIO("FPPOLED", new PLAT_GPIO_CLASS());
    }
    LoadSettings(argv[0]);
    int ledType = getRawSettingInt("LEDDisplayType", 99);
    LogInfo(VB_GENERAL, "    Led Type: %d\n", ledType);
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
