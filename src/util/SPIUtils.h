#pragma once

#include <stdint.h>

class SPIUtils {
public:
    SPIUtils(int channel, int baud);
    ~SPIUtils();
    
    
    int xfer(uint8_t *tx, uint8_t *rx, int count);
    
    bool isOk() { return file != -1;}
private:
    int file;
    int channel;
    int speed;
    int bitsPerWord;
};
