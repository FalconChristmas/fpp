
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <thread>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "I2CUtils.h"

// some of this is based off the pigpio.c file which is public domain


I2CUtils::I2CUtils(int bus, int address) {
    file = -1;
    char dev[64];
    sprintf(dev, "/dev/i2c-%d", bus);
    Init(dev, address);
}
I2CUtils::I2CUtils(const char * bus, int address) {
    file = -1;
    char dev[64];
    sprintf(dev, "/dev/%s", bus);
    Init(dev, address);
}
void I2CUtils::Init(const char *dev, int address) {
    if ((file = open(dev, O_RDWR)) < 0) {
        if (system("/sbin/modprobe i2c_dev") == -1) { /* ignore errors */}
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if ((file = open(dev, O_RDWR)) < 0) {
            file = -1;
            return;
        }
    }
    
    if (ioctl(file, I2C_SLAVE, address) < 0) {
        close(file);
        file = -1;
        return;
    }
    
    if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
        funcs = -1;
    }
}
I2CUtils::~I2CUtils() {
    if (file != -1) {
        close(file);
        file = -1;
    }
}


int I2CUtils::readDevice(uint8_t *buf, int count) {
    if (file != -1) {
        return read(file, buf, count);
    }
    return -1;
}
int I2CUtils::writeDevice(uint8_t *buf, int count) {
    if (file != -1) {
        return write(file, buf, count);
    }
    return -1;
}


inline int fpp_smbus_access(int file, char rw, uint8_t cmd, int size, union i2c_smbus_data *data) {
    struct i2c_smbus_ioctl_data args;
    args.read_write = rw;
    args.command    = cmd;
    args.size       = size;
    args.data       = data;
    
    return ioctl(file, I2C_SMBUS, &args);
}
int I2CUtils::readByte() {
    if (file != -1) {
        union i2c_smbus_data data;
        int status = fpp_smbus_access(file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
        if (status < 0) {
            return -1;
        }
        return 0xFF & data.byte;
    }
    return -1;
}
int I2CUtils::writeByte(unsigned val) {
    if (file != -1) {
        val = val & 0xFF;
        int status = fpp_smbus_access(file, I2C_SMBUS_WRITE, val, I2C_SMBUS_BYTE, NULL);
        if (status < 0) {
            return -1;
        }
        return status;
    }
    return -1;
}

int I2CUtils::readByteData(int reg) {
    if (file != -1) {
        union i2c_smbus_data data;
        int status = fpp_smbus_access(file, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data);
        
        if (status < 0) {
            return -1;
        }
        return 0xFF & data.byte;
    }
    return -1;
}
int I2CUtils::writeByteData(int reg, unsigned val) {
    if (file != -1) {
        union i2c_smbus_data data;
        data.byte = val & 0xFF;
        int status = fpp_smbus_access(file, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, &data);
        if (status < 0) {
            return -1;
        }
        return status;
    }
    return -1;
}
int I2CUtils::readWordData(int reg) {
    if (file != -1) {
        union i2c_smbus_data data;
        int status = fpp_smbus_access(file, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, &data);
        
        if (status < 0) {
            return -1;
        }
        return 0xFFFF & data.word;
    }
    return -1;
}
int I2CUtils::writeWordData(int reg, unsigned val) {
    if (file != -1) {
        union i2c_smbus_data data;
        data.word = val & 0xFFFF;
        int status = fpp_smbus_access(file, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, &data);
        if (status < 0) {
            return -1;
        }
        return status;
    }
    return -1;
}

int I2CUtils::writeBlockData(int reg, const uint8_t *buf, int count) {
    if (file != -1) {
        union i2c_smbus_data data;
        if (count > 32) count = 32; //max for SMBus
        data.block[0] = count;
        for (int x = 0; x < count; x++) {
            data.block[x+1] = buf[x];
        }
        int status = fpp_smbus_access(file, I2C_SMBUS_WRITE, reg, I2C_SMBUS_I2C_BLOCK_DATA, &data);
        if (status < 0) {
            return -1;
        }
        return status;
    }
    return -1;
}
int I2CUtils::writeI2CBlockData(int reg, const uint8_t *buf, int count) {
    if (file != -1) {
        union i2c_smbus_data data;
        if (count > 32) count = 32; //max
        data.block[0] = count;
        for (int x = 0; x < count; x++) {
            data.block[x+1] = buf[x];
        }
        int status = fpp_smbus_access(file, I2C_SMBUS_WRITE, reg, I2C_SMBUS_I2C_BLOCK_BROKEN, &data);
        if (status < 0) {
            return -1;
        }
        return status;
    }
    return -1;
}

int I2CUtils::readI2CBlockData(int reg, uint8_t *buf, int count) {
    if (file != -1) {
        union i2c_smbus_data data;
        if (count > 32) count = 32; //max
        data.block[0] = count;
        for (int x = 0; x < count; x++) {
            data.block[x+1] = buf[x];
        }
        int status = fpp_smbus_access(file, I2C_SMBUS_READ, reg, I2C_SMBUS_I2C_BLOCK_DATA, &data);
        if (status < 0) {
            return -1;
        }
        for (int i = 0; i < count; i++) {
            // Skip the first byte, which is the length of the rest of the block.
            buf[i] = data.block[i+1];
        }
        return count;
    }
    return -1;
}
