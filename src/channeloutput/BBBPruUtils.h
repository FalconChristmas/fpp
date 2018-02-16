#ifndef _bbb_pru_utils_h_
#define _bbb_pru_utils_h_

#include <stdio.h>
#include <inttypes.h>
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
    
    int run(const std::string &program);
    void stop();
    
    
    unsigned pru_num;
    
    uint8_t * data_ram;      // PRU data ram in ARM space
    size_t data_ram_size; // size in bytes of the PRU's data RAM
    
    uint8_t * shared_ram;      // PRU data ram in ARM space
    size_t shared_ram_size; // size in bytes of the PRU's data RAM

    uint8_t * other_data_ram;      // PRU data ram in ARM space
    size_t other_data_ram_size; // size in bytes of the PRU's data RAM

    uint8_t * ddr;           // PRU DMA address (in ARM space)
    uintptr_t ddr_addr;   // PRU DMA address (in PRU space)
    size_t ddr_size;      // Size in bytes of the shared space
};


#endif
