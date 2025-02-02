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

#include "fpp-pch.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>
#include <fcntl.h>
#include <filesystem>
#include <inttypes.h>
#include <stdint.h>
#include <thread>
#include <unistd.h>

#include "../common.h"
#include "../log.h"

#include "BBBPruUtils.h"

static unsigned int proc_read(const char* const fname) {
    FILE* const f = fopen(fname, "r");
    unsigned int x;
    fscanf(f, "%x", &x);
    fclose(f);
    return x;
}

class PRUCoreInfo {
public:
    int pru_num = 0;
    uint8_t* dataRam = nullptr;
    uintptr_t dataRamPhyLoc = 0;
    int dataRamSize = 0;
    uint8_t* sharedRam = nullptr;
    uint32_t sharedRamPhyLoc = 0;
    int sharedRamSize = 0;

    void disable() {
        std::string filename = "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
        if (FileExists(filename)) {
            FILE* rp = fopen(filename.c_str(), "w");
            fprintf(rp, "stop");
            fclose(rp);
        }
    }
    void enable(uint32_t addr = 0) {
        std::string filename = "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
        int cnt = 0;
        while (!FileExists(filename) && cnt < 10000) {
            cnt++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (FileExists(filename) && cnt < 10000) {
            FILE* rp = fopen(filename.c_str(), "w");
            fprintf(rp, "start");
            fclose(rp);
        } else {
            LogWarn(VB_CHANNELOUT, "BBBPru::Pru::enable() - could not start PRU core %d\n", pru_num);
        }
    }
};

static int prussUseCount = 0;

static uint8_t* ddr_mem_loc = nullptr;
static uint32_t ddr_phy_mem_loc = 0;
static size_t ddr_filelen = 0;
static uint8_t* base_memory_location = nullptr;
static PRUCoreInfo prus[2];

#if defined(PLATFORM_BBB)

constexpr uintptr_t PRUSS_MMAP_BASE = 0x4a300000;
constexpr size_t PRUSS_MMAP_SIZE = 0x40000;

constexpr uintptr_t PRUSS_DATARAM0_PHYS_BASE = 0x4a300000;
constexpr uintptr_t PRUSS_DATARAM1_PHYS_BASE = 0x4a302000;
constexpr size_t PRUSS_DATARAM_SIZE = 8 * 1024; // 8K

constexpr uintptr_t PRUSS_SHAREDRAM_BASE = 0x4a310000;
constexpr size_t PRUSS_SHAREDRAM_SIZE = 12 * 1024; // 12K

constexpr uintptr_t DDR_ADDR = 0x8f000000;
constexpr uintptr_t DDR_SIZE = 0x00400000;

constexpr std::string FIRMWARE_PREFIX = "am335x";
constexpr bool FAKE_PRU = false;

#elif defined(PLATFORM_BB64)
constexpr uintptr_t PRUSS_MMAP_BASE = 0x30040000;
constexpr size_t PRUSS_MMAP_SIZE = 0x80000;

constexpr size_t PRUSS_DATARAM_SIZE = 8 * 1024; // 8K
constexpr uintptr_t PRUSS_DATARAM0_PHYS_BASE = 0x30040000;
constexpr uintptr_t PRUSS_DATARAM1_PHYS_BASE = 0x30042000;

constexpr uintptr_t PRUSS_SHAREDRAM_BASE = 0x30050000;
constexpr size_t PRUSS_SHAREDRAM_SIZE = 32 * 1024; // 32K

// constexpr uintptr_t DDR_ADDR = 0x8f000000;
// constexpr uintptr_t DDR_SIZE = 0x00400000;

constexpr uintptr_t DDR_ADDR = 0;
constexpr uintptr_t DDR_SIZE = 0x00400000;
constexpr std::string FIRMWARE_PREFIX = "am62x";

constexpr bool FAKE_PRU = true;
#endif

static void initPrus() {
    const int mem_fd = open("/dev/mem", O_RDWR);

    if (!FAKE_PRU) {
        base_memory_location = (uint8_t*)mmap(0,
                                              PRUSS_MMAP_SIZE,
                                              PROT_WRITE | PROT_READ,
                                              MAP_SHARED,
                                              mem_fd,
                                              PRUSS_MMAP_BASE);
    } else {
        base_memory_location = (uint8_t*)mmap(0,
                                              PRUSS_MMAP_SIZE,
                                              PROT_WRITE | PROT_READ,
                                              MAP_PRIVATE | MAP_ANONYMOUS,
                                              0,
                                              0);
    }

    uintptr_t ddr_addr = DDR_ADDR;
    uintptr_t ddr_sizeb = DDR_SIZE;

    if (!FileExists("/sys/class/remoteproc/remoteproc0/state")) {
        system("modprobe pru_rproc");
    }
    if (ddr_mem_loc == nullptr && ddr_addr) {
        ddr_phy_mem_loc = ddr_addr;
        ddr_filelen = ddr_sizeb;
        ddr_mem_loc = (uint8_t*)mmap(0,
                                     ddr_filelen,
                                     PROT_WRITE | PROT_READ,
                                     MAP_SHARED,
                                     mem_fd,
                                     ddr_addr);
    } else if (ddr_addr == 0) {
        // just malloc some memory so we don't crash
        ddr_phy_mem_loc = ddr_addr;
        ddr_filelen = ddr_sizeb;
        ddr_mem_loc = (uint8_t*)mmap(0,
                                     ddr_filelen,
                                     PROT_WRITE | PROT_READ,
                                     MAP_PRIVATE | MAP_ANONYMOUS,
                                     0,
                                     0);
    }
    close(mem_fd);

    prus[0].pru_num = 0;
    prus[1].pru_num = 1;
    prus[0].dataRam = base_memory_location + (PRUSS_DATARAM0_PHYS_BASE - PRUSS_MMAP_BASE);
    prus[0].dataRamSize = PRUSS_DATARAM_SIZE;
    prus[0].dataRamPhyLoc = PRUSS_DATARAM0_PHYS_BASE;
    prus[1].dataRam = base_memory_location + (PRUSS_DATARAM1_PHYS_BASE - PRUSS_MMAP_BASE);
    prus[1].dataRamSize = PRUSS_DATARAM_SIZE;
    prus[1].dataRamPhyLoc = PRUSS_DATARAM1_PHYS_BASE;
    prus[0].sharedRam = base_memory_location + (PRUSS_SHAREDRAM_BASE - PRUSS_MMAP_BASE);
    prus[0].sharedRamSize = PRUSS_SHAREDRAM_SIZE;
    prus[0].sharedRamPhyLoc = PRUSS_SHAREDRAM_BASE;
    prus[1].sharedRam = base_memory_location + (PRUSS_SHAREDRAM_BASE - PRUSS_MMAP_BASE);
    prus[1].sharedRamSize = PRUSS_SHAREDRAM_SIZE;
    prus[1].sharedRamPhyLoc = PRUSS_SHAREDRAM_BASE;

    if (ddr_sizeb) {
        memset(ddr_mem_loc, 0, ddr_sizeb);
    }
    __asm__ __volatile__("" ::
                             : "memory");
}

BBBPru::BBBPru(int pru, bool mapShared, bool mapOther) :
    pru_num(pru) {
    if (prussUseCount == 0) {
        initPrus();
    }
    prussUseCount++;

    this->data_ram = prus[pru].dataRam;
    this->data_ram_size = prus[pru].dataRamSize;

    this->ddr = ddr_mem_loc;
    this->ddr_size = ddr_filelen;
    this->ddr_addr = ddr_phy_mem_loc;

    if (mapShared) {
        this->shared_ram_size = prus[pru].sharedRamSize;
        this->shared_ram = prus[pru].sharedRam;
    } else {
        this->shared_ram_size = 0;
        this->shared_ram = 0;
    }
    if (mapOther) {
        int p = pru ? 0 : 1;
        this->other_data_ram = prus[p].dataRam;
        this->other_data_ram_size = prus[p].dataRamSize;
    } else {
        this->other_data_ram_size = 0;
        this->other_data_ram = 0;
    }
}

BBBPru::~BBBPru() {
    prussUseCount--;
    if (prussUseCount == 0) {
        if (ddr_mem_loc) {
            munmap(ddr_mem_loc, ddr_filelen);
            ddr_mem_loc = nullptr;
        }
        if (base_memory_location) {
            munmap(base_memory_location, PRUSS_MMAP_SIZE);
            base_memory_location = nullptr;
        }
    }
}

int BBBPru::run(const std::string& program) {
    LogDebug(VB_CHANNELOUT, "BBBPru[%d]::run(%s)\n", pru_num, program.c_str());

    prus[pru_num].disable();
    CopyFileContents(program, "/lib/firmware/" + FIRMWARE_PREFIX + "-pru" + std::to_string(pru_num) + "-fw");
    prus[pru_num].enable();

    memset(prus[pru_num].dataRam, 0, prus[pru_num].dataRamSize);
    memset(prus[pru_num].dataRam, 0, prus[pru_num].dataRamSize);

    return false;
}

void BBBPru::stop() {
    prus[pru_num].disable();
}
