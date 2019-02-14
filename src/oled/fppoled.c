
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "I2C.h"
#include "SSD1306_OLED.h"
#include "FPPOLEDUtils.h"

void sigHandler(int sig) {
    clearDisplay();
    Display();
    exit(1);
}


int main (int argc, char *argv[]) {
    
    if (init_i2c_dev2(SSD1306_OLED_ADDR) != 0) {
        printf("(Main)i2c-2: OOPS! Something Went Wrong\n");
        exit(1);
    }
    
    if (display_Init_seq() )  {
        printf("Could not initialize display\n");
        exit(1);
    }
    
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    
    sigaction(SIGINT, &sigIntHandler, NULL);

    
    int count = 0;
    FPPOLEDUtils oled(0);
    while (true) {
        oled.doIteration(count);
        count++;
        sleep(1);
    }
}
