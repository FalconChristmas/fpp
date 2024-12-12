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

#include <inttypes.h>
#include <stdio.h>
#include <string>

/**
 * Mapping of the PRU memory spaces.
 *
 * The PRU has a 8K, fast local data RAM that is mapped into ARM memory,
 * as well as a 12K, fast local data RAM that is shared for both PRU's
 * as well as slower access to the DDR RAM of the ARM.
 */
class BBBPru {
public:
    BBBPru(int pru, bool mapShared = false, bool mapOther = false);
    ~BBBPru();

    int run(const std::string& program);
    void stop();

    void clearPRUMem(uint8_t *ptr, size_t sz);
    void memcpyToPRU(uint8_t *dst, uint8_t*src, size_t sz);

    unsigned pru_num;

    uint8_t* data_ram;    // PRU data ram in ARM space
    size_t data_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* shared_ram;    // PRU data ram in ARM space
    size_t shared_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* other_data_ram;    // PRU data ram in ARM space
    size_t other_data_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* ddr;       // PRU DMA address (in ARM space)
    uint32_t ddr_addr; // PRU DMA address (in PRU space)
    size_t ddr_size;    // Size in bytes of the shared space
};
