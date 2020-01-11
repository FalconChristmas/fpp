#ifndef __FPP_I2C_UTILS__
#define __FPP_I2C_UTILS__

#include <stdint.h>

class I2CUtils {
public:
    I2CUtils(int bus, int address);
    I2CUtils(const char *i2cdev, int address);
    ~I2CUtils();

    int readDevice(uint8_t *buf, int count);
    int writeDevice(uint8_t *buf, int count);

    int readByte();
    int writeByte(unsigned val);
    
    int readByteData(int reg);
    int writeByteData(int reg, unsigned val);
    int readWordData(int reg);
    int writeWordData(int reg, unsigned val);
    
    int writeBlockData(int reg, const uint8_t *buf, int count);
    int writeI2CBlockData(int reg, const uint8_t *buf, int count);
    int readI2CBlockData(int reg, uint8_t *buf, int count);

    bool isOk() { return file != -1;}
private:
    void Init(const char *, int address);
    int file;
    unsigned long funcs;
};

#endif
