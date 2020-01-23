#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>


#include "log.h"
#include "BBBPruUtils.h"


static unsigned int proc_read(const char * const fname) {
    FILE * const f = fopen(fname, "r");
    unsigned int x;
    fscanf(f, "%x", &x);
    fclose(f);
    return x;
}

static int prussUseCount = 0;
static uint8_t *ddr_mem_loc = nullptr;
static size_t ddr_filelen = 0;

BBBPru::BBBPru(int pru, bool mapShared, bool mapOther) : pru_num(pru) {
    if (prussUseCount == 0) {
        prussdrv_init();
        prussdrv_open(PRU_EVTOUT_0);
    }
    if (pru) {
        prussdrv_open(PRU_EVTOUT_1);
    }
    prussUseCount++;
    tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
    prussdrv_pruintc_init(&pruss_intc_initdata);
    
    void * pru_data_mem;
    prussdrv_map_prumem(pru_num == 0 ? PRUSS0_PRU0_DATARAM :PRUSS0_PRU1_DATARAM,
                        &pru_data_mem
                        );

    const int mem_fd = open("/dev/mem", O_RDWR);
    const uintptr_t ddr_addr = proc_read("/sys/class/uio/uio0/maps/map1/addr");
    const uintptr_t ddr_sizeb = proc_read("/sys/class/uio/uio0/maps/map1/size");
    
    
    if (ddr_mem_loc == nullptr) {
        ddr_filelen = ddr_sizeb;
        ddr_mem_loc = (uint8_t*)mmap(0,
                                     ddr_filelen,
                                     PROT_WRITE | PROT_READ,
                                     MAP_SHARED,
                                     mem_fd,
                                     ddr_addr
                                     );
    }
    
    close(mem_fd);
    this->ddr_addr = ddr_addr;
    this->data_ram = (uint8_t*)pru_data_mem;
    this->data_ram_size = 8192;
    this->ddr = ddr_mem_loc;
    this->ddr_size = ddr_sizeb;
    
    if (mapShared) {
        prussdrv_map_prumem(PRUSS0_SHARED_DATARAM,
                            (void**)&this->shared_ram);
        this->shared_ram_size = 12 * 1024;
    } else {
        this->shared_ram_size = 0;
        this->shared_ram = 0;
    }
    if (mapOther) {
        prussdrv_map_prumem(pru_num == 1 ? PRUSS0_PRU0_DATARAM :PRUSS0_PRU1_DATARAM,
                            (void**)&this->other_data_ram);
        this->other_data_ram_size = 8192;
    } else {
        this->other_data_ram_size = 0;
        this->other_data_ram = 0;
    }
}

BBBPru::~BBBPru() {
    prussUseCount--;
    if (prussUseCount == 0) {
        prussdrv_exit();
        
        if (ddr_mem_loc) {
            munmap(ddr_mem_loc, ddr_filelen);
            ddr_mem_loc = nullptr;
        }
    }
}

int BBBPru::run(const std::string &program) {
    char * program_unconst = (char*)(uintptr_t) program.c_str();
    int ret = prussdrv_exec_program(pru_num, program_unconst);
    LogDebug(VB_CHANNELOUT, "BBBPru::run(%s) -> %d\n", program_unconst, ret);
    return ret;
}
void BBBPru::stop(bool force) {
    if (!force) {
        prussdrv_pru_wait_event(pru_num == 0 ? PRU_EVTOUT0 : PRU_EVTOUT1);
    }
    prussdrv_pru_clear_event(pru_num == 0 ? PRU_EVTOUT0 : PRU_EVTOUT1,
                             pru_num == 0 ? PRU0_ARM_INTERRUPT : PRU1_ARM_INTERRUPT);
    prussdrv_pru_disable(pru_num);
}


