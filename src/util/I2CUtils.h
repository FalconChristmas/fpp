#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <stdint.h>

class I2CUtils {
public:
    I2CUtils(int bus, int address);
    I2CUtils(const char* i2cdev, int address);
    ~I2CUtils();

    int readDevice(uint8_t* buf, int count);
    int writeDevice(uint8_t* buf, int count);

    int readByte();
    int writeByte(unsigned val);

    int readByteData(int reg);
    int writeByteData(int reg, unsigned val);
    int readWordData(int reg);
    int writeWordData(int reg, unsigned val);

    // these are limitted to 32bytes due to smbus protocol restrictions
    int writeBlockData(int reg, const uint8_t* buf, int count);
    int writeI2CBlockData(int reg, const uint8_t* buf, int count);
    int readI2CBlockData(int reg, uint8_t* buf, int count);

    // this writes the bytes directly via i2c and would be limitted to the
    // 254 bytes data (+byte for len +byte for reg)
    int writeRawI2CBlockData(int reg, const uint8_t* buf, int count);

    bool isOk() { return file != -1; }

private:
    void Init(const char*, int address);
    int file;
    int address;
    unsigned long funcs;
};
