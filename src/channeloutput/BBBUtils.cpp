
#include "BBBUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


int configBBBPin(int gpio, int pin, const char *direction) {
    const unsigned pin_num = gpio * 32 + pin;
    const char * export_name = "/sys/class/gpio/export";

    
    char dir_name[64];
    snprintf(dir_name, sizeof(dir_name),
             "/sys/class/gpio/gpio%u/direction",
             pin_num
             );
    
    FILE * const dir = fopen(dir_name, "w");
    if (!dir) {
        return -1;
    }
    fprintf(dir, "%s\n", direction);
    fclose(dir);
}


