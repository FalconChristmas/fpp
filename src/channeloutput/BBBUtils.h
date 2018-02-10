#ifndef __BBB_UTILS__
#define __BBB_UTILS__

#include <BBBPruUtils.h>


int configBBBPin(const char *name, int gpio, int pin, const char *mode);

enum BeagleBoneType {
    Black,
    BlackWireless,
    Green,
    GreenWireless,
    PocketBeagle
};

BeagleBoneType getBeagleBoneType();

#endif
