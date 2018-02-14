
#include "BBBUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


int configBBBPin(const char *name, int gpio, int pin, const char *mode)
{
    char dir_name[128];
    snprintf(dir_name, sizeof(dir_name),
         "/sys/devices/platform/ocp/ocp:%s_pinmux/state",
         name
         );
    FILE *dir = fopen(dir_name, "w");
    if (!dir) {
        return -1;
    }
    fprintf(dir, "%s\n", mode);
    fclose(dir);


    const unsigned pin_num = gpio * 32 + pin;
    snprintf(dir_name, sizeof(dir_name),
             "/sys/class/gpio/gpio%u/direction",
             pin_num
             );
    
    dir = fopen(dir_name, "w");
    if (!dir) {
        return -1;
    }
    fprintf(dir, "out\n");
    fclose(dir);
    return 0;
}

BeagleBoneType getBeagleBoneType() {
    FILE *file = fopen("/proc/device-tree/model", "r");
    if (file) {
        char buf[256];
        fgets(buf , 256 , file);
        fclose(file);
        if (strcmp(&buf[10], "PocketBeagle") == 0) {
            return PocketBeagle;
        } else if (strcmp(&buf[10], "BeagleBone Black Wireless") == 0) {
            return BlackWireless;
        } else if (strcmp(&buf[10], "BeagleBone Green Wireless") == 0) {
            return GreenWireless;
        } else if (strcmp(&buf[10], "BeagleBone Green") == 0) {
            return Green;
        }
    }

    return Black;
}



