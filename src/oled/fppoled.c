
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

void sigInteruptHandler(int sig) {
    clearDisplay();
    Display();
    exit(1);
}

void sigTermHandler(int sig) {
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
    if (ledType == 0) {
        exit(0);
    }
    ledType -= 1;
    SSD1306_LCDWIDTH = 128;
    if (ledType & 0x1) {
        SSD1306_LCDHEIGHT = 32;
    } else {
        SSD1306_LCDHEIGHT = 64;
    }
    
    if (init_i2c_dev2(I2C_DEV_PATH, SSD1306_OLED_ADDR) != 0) {
        printf("(Main)i2c-2: OOPS! Something Went Wrong\n");
        exit(1);
    }
    
    if (display_Init_seq() )  {
        printf("Could not initialize display\n");
        exit(1);
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
    FPPOLEDUtils oled(ledType);
    while (true) {
        oled.doIteration(count);
        count++;
        sleep(1);
    }
}
