
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "I2C.h"
#include "SSD1306_OLED.h"
#include "FPPOLEDUtils.h"
#include "settings.h"
#include "common.h"


static FPPOLEDUtils *oled = nullptr;
void sigInteruptHandler(int sig) {
    oled->cleanup();
    clearDisplay();
    Display();
    exit(1);
}

void sigTermHandler(int sig) {
    oled->cleanup();
    clearDisplay();
    Display();
    exit(0);
}

#ifdef PLATFORM_BBB
#define I2C_DEV_PATH I2C_DEV2_PATH
#else
#define I2C_DEV_PATH I2C_DEV1_PATH
#endif


int main (int argc, char *argv[]) {
    printf("FPP OLED Status Display Driver\n");
    initSettings(argc, argv);
    if (DirectoryExists("/home/fpp")) {
        printf("    Loading Settings\n");
        loadSettings("/home/fpp/media/settings");
    }
    int ledType = getSettingInt("LEDDisplayType");
    printf("    Led Type: %d\n", ledType);
    fflush(stdout);
    LED_DISPLAY_WIDTH = 128;
    if (ledType == 3 || ledType == 4) {
        LED_DISPLAY_HEIGHT = 32;
    } else {
        LED_DISPLAY_HEIGHT = 64;
    }

    if (ledType == 5 || ledType == 6) {
        LED_DISPLAY_TYPE = LED_DISPLAY_TYPE_SH1106;
    }
    
    if (init_i2c_dev2(I2C_DEV_PATH, SSD1306_OLED_ADDR) != 0) {
        printf("(Main)i2c: OOPS! Something Went Wrong\n");
        exit(1);
    }
    
    if (ledType && display_Init_seq() )  {
        printf("Could not initialize display\n");
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
