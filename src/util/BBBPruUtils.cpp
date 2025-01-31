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
    uintptr_t sharedRamPhyLoc = 0;
    int sharedRamSize = 0;

    void disable() {
        std::string filename = "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
        if (FileExists(filename)) {
            FILE* rp = fopen(filename.c_str(), "w");
            fprintf(rp, "stop");
            fclose(rp);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::string f = GetFileContents(filename);
            TrimWhiteSpace(f);
            int cnt = 0;
            while (f != "offline" && cnt < 500) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                f = GetFileContents(filename);
                TrimWhiteSpace(f);
                cnt++;
            }
            if (f != "offline") {
                LogWarn(VB_CHANNELOUT, "BBBPru::Pru::disable() - could not stop PRU core %d   State: %s\n", pru_num, f.c_str());
            }
        }
    }
    void enable() {
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
            cnt = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::string f = GetFileContents(filename);
            TrimWhiteSpace(f);
            while (f != "running" && cnt < 500) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                f = GetFileContents(filename);
                TrimWhiteSpace(f);
                cnt++;
            }
            if (f != "running") {
                LogWarn(VB_CHANNELOUT, "BBBPru::Pru::enable() - could not start PRU core %d    State: %s\n", pru_num, f.c_str());
            }
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

constexpr uint32_t DDR_ADDR = 0x8f000000;
constexpr size_t DDR_SIZE = 0x00400000;

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

constexpr uint32_t DDR_ADDR = 0x8f000000;
constexpr size_t DDR_SIZE = 0x00400000;

constexpr std::string FIRMWARE_PREFIX = "am62x";

static bool FAKE_PRU = !FileExists("/sys/class/remoteproc/remoteproc0/state");
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

    uint32_t ddr_addr = DDR_ADDR;
    size_t ddr_sizeb = DDR_SIZE;

    if (ddr_mem_loc == nullptr && !FAKE_PRU) {
        ddr_phy_mem_loc = ddr_addr;
        ddr_filelen = ddr_sizeb;
        ddr_mem_loc = (uint8_t*)mmap(0,
                                     ddr_filelen,
                                     PROT_WRITE | PROT_READ,
                                     MAP_SHARED,
                                     mem_fd,
                                     ddr_addr);
    } else if (ddr_addr == 0 || FAKE_PRU) {
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

    if (!FAKE_PRU) {
        prus[pru_num].disable();
        CopyFileContents(program, "/lib/firmware/" + FIRMWARE_PREFIX + "-pru" + std::to_string(pru_num) + "-fw");
        prus[pru_num].enable();
    } else {
        CopyFileContents(program, "/lib/firmware/" + FIRMWARE_PREFIX + "-pru" + std::to_string(pru_num) + "-fw");
    }

    clearPRUMem(data_ram, data_ram_size);
    if (shared_ram) {
        clearPRUMem(shared_ram, shared_ram_size);
    }
    if (other_data_ram) {
        clearPRUMem(other_data_ram, other_data_ram_size);
    }

    /*
    printf("DL:  %p\n", data_ram);
    printf("OL:  %p\n", other_data_ram);
    printf("SH:  %p\n", shared_ram);
    printf("DDR:  %p\n", ddr);
    */
    return false;
}

#ifdef PLATFORM_BBB
void BBBPru::clearPRUMem(uint8_t* ptr, size_t sz) {
    memset(ptr, 0, sz);
}
void BBBPru::memcpyToPRU(uint8_t* dst, uint8_t* src, size_t sz) {
    memcpy(dst, src, sz);
}
#elif defined(PLATFORM_BB64)
#pragma GCC push_options
#pragma GCC optimize("O0")
// The optimized memset and memcpy on Arm64 will segfault
// when doing certain sized operations to un-cacheable
// ram segments.  Need to use ldnp/stnp instructions
void memcpy_ldnp(volatile unsigned char* dst, volatile unsigned char* src, int sztotal) {
    int sz = sztotal - sztotal & 64;

    for (int x = sz; x < sztotal; x++) {
        dst[x] = src[x];
    }
    asm volatile(
        "NEONCopyPLD: \n"
        "sub %[dst], %[dst], #64 \n"
        "1: \n"
        "ldnp q0, q1, [%[src]] \n"
        "ldnp q2, q3, [%[src], #32] \n"
        "add %[dst], %[dst], #64 \n"
        "subs %[sz], %[sz], #64 \n"
        "add %[src], %[src], #64 \n"
        "stnp q0, q1, [%[dst]] \n"
        "stnp q2, q3, [%[dst], #32] \n"
        "b.gt 1b \n"
        : [dst] "+r"(dst), [src] "+r"(src), [sz] "+r"(sz) : : "q0", "q1", "q2", "q3", "cc", "memory");
}

void BBBPru::clearPRUMem(uint8_t* ptr, size_t sz) {
    __uint128_t z = 0;
    int c = sz / sizeof(z);
    __uint128_t* p = (__uint128_t*)ptr;
    for (int x = 0; x < c; ++x) {
        *p = z;
        p++;
    }
    for (int x = c * sizeof(z); x < sz; ++x) {
        ptr[x] = 0;
    }
}
void BBBPru::memcpyToPRU(uint8_t* dst, uint8_t* src, size_t sz) {
    memcpy_ldnp(dst, src, sz);
}
#pragma GCC pop_options
#endif

void BBBPru::stop() {
    if (!FAKE_PRU) {
        prus[pru_num].disable();
    }
}
