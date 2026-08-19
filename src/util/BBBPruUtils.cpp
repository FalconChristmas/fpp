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
#include <algorithm>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <filesystem>
#include <inttypes.h>
#include <map>
#include <mutex>
#include <stdint.h>
#include <thread>
#include <unistd.h>
#include <vector>

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

    uint8_t* sram = nullptr;
    uint32_t sram_addr = 0;
    size_t sram_size = 0;

    uint8_t* m4ram = nullptr;
    uint32_t m4ram_addr = 0;
    size_t m4ram_size = 0;

    std::string getRemoteProcStateFile() {
#ifdef PLATFORM_BBB
        return "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
#elif defined(PLATFORM_BB64)
        std::string base;
        if (pru_num == 0) {
            base = "/sys/devices/platform/bus@f0000/30040000.pruss/30074000.pru/remoteproc/remoteproc";
        } else if (pru_num == 1) {
            base = "/sys/devices/platform/bus@f0000/30040000.pruss/30078000.pru/remoteproc/remoteproc";
        } else {
            // M4 core, not PRU, but startable via remoteproc
            base = "/sys/devices/platform/bus@f0000/bus@f0000:bus@4000000/5000000.m4fss/remoteproc/remoteproc";
        }
        for (int x = 0; x < 10; x++) {
            if (FileExists(base + std::to_string(x) + "/state")) {
                return base + std::to_string(x) + "/state";
            }
        }
        return "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
#endif
    }
    void disable() {
        std::string filename = getRemoteProcStateFile();
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
    bool enable() {
        std::string filename = getRemoteProcStateFile();
        int cnt = 0;
        while (!FileExists(filename) && cnt < 10000) {
            cnt++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (FileExists(filename) && cnt < 10000) {
            if (pru_num <= 1) {
                // explicitly select the firmware image; the sysfs setting is
                // sticky so anything else that loaded firmware on this core
                // (tests, other tools) would otherwise be re-booted instead
                std::string fwFile = filename.substr(0, filename.size() - 5) + "firmware";
                FILE* fw = fopen(fwFile.c_str(), "w");
                if (fw) {
#if defined(PLATFORM_BBB)
                    fprintf(fw, "am335x-pru%d-fw", pru_num);
#else
                    fprintf(fw, "am62x-pru%d-fw", pru_num);
#endif
                    fclose(fw);
                }
            }
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
                return false;
            }
        } else {
            LogWarn(VB_CHANNELOUT, "BBBPru::Pru::enable() - could not start PRU core %d\n", pru_num);
            return false;
        }
        return true;
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

static uint8_t* PRUSS_SRAM_BASE = 0x00000000;
constexpr uint32_t PRUSS_SRAM_GLOBAL = 0x00000000;
constexpr size_t PRUSS_SRAM_SIZE = 0;

static uint8_t* PRUSS_M4RAM_BASE = 0x00000000;
constexpr uint32_t PRUSS_M4RAM_GLOBAL = 0x00000000;
constexpr size_t PRUSS_M4RAM_SIZE = 0;

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

// LDI32    r8,  0x70000000   //  OCSRAM
// LDI32    r8,  0x0005040000  // M4 DRAM

static uint8_t* PRUSS_SRAM_BASE = nullptr;
constexpr uint32_t PRUSS_SRAM_GLOBAL = 0x70000000;
constexpr size_t PRUSS_SRAM_SIZE = 64 * 1024; // 64K

static uint8_t* PRUSS_M4RAM_BASE = nullptr;
constexpr uint32_t PRUSS_M4RAM_GLOBAL = 0x05040000;
constexpr size_t PRUSS_M4RAM_SIZE = 256 * 1024; // 256K;

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
        if (PRUSS_SRAM_SIZE > 0) {
            PRUSS_SRAM_BASE = (uint8_t*)mmap(0,
                                             PRUSS_SRAM_SIZE,
                                             PROT_WRITE | PROT_READ,
                                             MAP_SHARED,
                                             mem_fd,
                                             PRUSS_SRAM_GLOBAL);
        }
        if (PRUSS_M4RAM_SIZE > 0) {
            PRUSS_M4RAM_BASE = (uint8_t*)mmap(0,
                                              PRUSS_M4RAM_SIZE,
                                              PROT_WRITE | PROT_READ,
                                              MAP_SHARED,
                                              mem_fd,
                                              PRUSS_M4RAM_GLOBAL);
        }
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
        madvise(ddr_mem_loc, ddr_filelen, MADV_HUGEPAGE);
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

    for (int x = 0; x < 2; x++) {
        prus[x].sram = PRUSS_SRAM_BASE;
        prus[x].sram_addr = PRUSS_SRAM_GLOBAL;
        prus[x].sram_size = PRUSS_SRAM_SIZE;

        prus[x].m4ram = PRUSS_M4RAM_BASE;
        prus[x].m4ram_addr = PRUSS_M4RAM_GLOBAL;
        prus[x].m4ram_size = PRUSS_M4RAM_SIZE;
    }

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

    this->sram = prus[pru].sram;
    this->sram_addr = prus[pru].sram_addr;
    this->sram_size = prus[pru].sram_size;

    this->m4ram = prus[pru].m4ram;
    this->m4ram_addr = prus[pru].m4ram_addr;
    this->m4ram_size = prus[pru].m4ram_size;
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
        if (PRUSS_SRAM_BASE) {
            munmap(PRUSS_SRAM_BASE, PRUSS_SRAM_SIZE);
            PRUSS_SRAM_BASE = nullptr;
        }
        if (PRUSS_M4RAM_BASE) {
            munmap(PRUSS_M4RAM_BASE, PRUSS_M4RAM_SIZE);
            PRUSS_M4RAM_BASE = nullptr;
        }
    }
}

static std::string pruFirmwareName(int pru_num) {
    return FIRMWARE_PREFIX + "-pru" + std::to_string(pru_num) + "-fw";
}

// /lib/firmware/<prefix>-pruN-fw ships as a symlink to the "-fw.sleep"
// parking firmware that the image loads at boot.  Copying the program
// through that name would overwrite the sleep firmware itself, so install
// the program under its own name and atomically repoint the link at it.
static void installPRUFirmware(int pru_num, const std::string& program) {
    std::string fw = "/lib/firmware/" + pruFirmwareName(pru_num);
    CopyFileContents(program, fw + ".fpp");
    std::string tmpLink = fw + ".new";
    unlink(tmpLink.c_str());
    if (symlink((pruFirmwareName(pru_num) + ".fpp").c_str(), tmpLink.c_str()) == 0) {
        rename(tmpLink.c_str(), fw.c_str());
    }
}

// Point the firmware name back at the sleep firmware so the next boot parks
// the PRU instead of re-running the last output program.  FalconSerial in
// particular free-runs frames without waiting for a first command, so leaving
// it installed as the boot firmware blasts garbage on any muxed pins.
static void restorePRUSleepFirmware(int pru_num) {
    std::string sleepName = pruFirmwareName(pru_num) + ".sleep";
    if (!FileExists("/lib/firmware/" + sleepName)) {
        return;
    }
    std::string fw = "/lib/firmware/" + pruFirmwareName(pru_num);
    std::string tmpLink = fw + ".new";
    unlink(tmpLink.c_str());
    if (symlink(sleepName.c_str(), tmpLink.c_str()) == 0) {
        rename(tmpLink.c_str(), fw.c_str());
    }
}

bool BBBPru::run(const std::string& program, bool clearSharedMems) {
    LogDebug(VB_CHANNELOUT, "BBBPru[%d]::run(%s)\n", pru_num, program.c_str());

    bool enabled = true;
    if (!FAKE_PRU) {
        prus[pru_num].disable();
        installPRUFirmware(pru_num, program);
        enabled = prus[pru_num].enable();
        if (!enabled) {
            return false;
        }
    } else {
        installPRUFirmware(pru_num, program);
    }
    clearPRUMem(data_ram, data_ram_size);
    if (clearSharedMems) {
        if (shared_ram) {
            clearPRUMem(shared_ram, shared_ram_size);
        }
        if (other_data_ram) {
            clearPRUMem(other_data_ram, other_data_ram_size);
        }
    }
    /*
    printf("DL:  %p\n", data_ram);
    printf("OL:  %p\n", other_data_ram);
    printf("SH:  %p\n", shared_ram);
    printf("DDR:  %p\n", ddr);
    */
    return true;
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
//
// The block loop moves whole 64 byte units and needs 16 byte aligned operands
// (the PRU memories map uncached, where an unaligned vector access faults), so
// the head/tail and anything that does not qualify go through the byte loop
// instead - slower, but correct for every size and alignment.  Exactly sztotal
// bytes are written and no more.
void memcpy_ldnp(volatile unsigned char* dst, volatile unsigned char* src, int sztotal) {
    // NB: "sztotal - sztotal & 64" parses as "(sztotal - sztotal) & 64", i.e.
    // always zero, which left the block loop below writing one fixed 64 byte
    // store - past the end for anything shorter - and everything else crawling
    // through the byte loop.  Mask off the low bits instead.
    int sz = 0;
    if (sztotal >= 64 && ((((uintptr_t)dst | (uintptr_t)src) & 15) == 0)) {
        sz = sztotal & ~63;
    }
    for (int x = sz; x < sztotal; x++) {
        dst[x] = src[x];
    }
    if (sz == 0) {
        return;
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

static std::mutex ddrAllocLock;
// owner -> (offset, size) within the reserved region
static std::map<std::string, std::pair<size_t, size_t>> ddrAllocs;

uint8_t* BBBPru::ddrAlloc(const std::string& owner, size_t size, uint32_t& physAddr, size_t minOffset) {
    std::unique_lock<std::mutex> lock(ddrAllocLock);
    if (ddr_mem_loc == nullptr) {
        LogErr(VB_CHANNELOUT, "BBBPru::ddrAlloc(%s): DDR region not mapped yet\n", owner.c_str());
        return nullptr;
    }
    size = (size + 4095) & ~(size_t)4095;
    size_t pos = (minOffset + 4095) & ~(size_t)4095;
    ddrAllocs.erase(owner);
    std::vector<std::pair<size_t, size_t>> used;
    for (auto& a : ddrAllocs) {
        used.push_back(a.second);
    }
    std::sort(used.begin(), used.end());
    for (auto& u : used) {
        if (pos + size <= u.first) {
            break;
        }
        if (u.first + u.second > pos) {
            pos = u.first + u.second;
        }
    }
    if (pos + size > ddr_filelen) {
        LogErr(VB_CHANNELOUT, "BBBPru::ddrAlloc(%s): %zuKB does not fit in the PRU DDR region (%zuKB of %zuKB already in use)\n",
               owner.c_str(), size / 1024, pos / 1024, ddr_filelen / 1024);
        return nullptr;
    }
    ddrAllocs[owner] = { pos, size };
    physAddr = ddr_phy_mem_loc + pos;
    LogInfo(VB_CHANNELOUT, "BBBPru: DDR region: '%s' at 0x%zX-0x%zX (%zuKB)\n",
            owner.c_str(), pos, pos + size, size / 1024);
    return ddr_mem_loc + pos;
}

void BBBPru::ddrRelease(const std::string& owner) {
    std::unique_lock<std::mutex> lock(ddrAllocLock);
    ddrAllocs.erase(owner);
}

void BBBPru::stop() {
    if (!FAKE_PRU) {
        prus[pru_num].disable();
        restorePRUSleepFirmware(pru_num);
    }
}

// The core control register sits 0x22000 into the PRUSS mapping, one 0x2000
// block per core, on both the AM335x and the AM62x.  Bit 1 enables the core.
static constexpr uintptr_t PRU_CTRL_OFFSET = 0x22000;
static constexpr uintptr_t PRU_CTRL_STRIDE = 0x2000;
static constexpr uint32_t PRU_CTRL_ENABLE = 0x2;

static volatile uint32_t* pruControlReg(unsigned pru_num) {
    if (!base_memory_location) {
        return nullptr;
    }
    return (volatile uint32_t*)(base_memory_location + PRU_CTRL_OFFSET + (pru_num * PRU_CTRL_STRIDE));
}

void BBBPru::pause() {
    if (FAKE_PRU) {
        return;
    }
    volatile uint32_t* ctrl = pruControlReg(pru_num);
    if (ctrl) {
        *ctrl = *ctrl & ~PRU_CTRL_ENABLE;
        __sync_synchronize();
    }
}

void BBBPru::resume() {
    if (FAKE_PRU) {
        return;
    }
    volatile uint32_t* ctrl = pruControlReg(pru_num);
    if (ctrl) {
        *ctrl = *ctrl | PRU_CTRL_ENABLE;
        __sync_synchronize();
    }
}

#include "../pru/SMEMRing.hp"

void BBBPruSMEMRing::attach(BBBPru* pru, uint32_t pruBaseAddr, uint32_t ringSize, bool usePointerMode) {
    basePru = pruBaseAddr;
    size = ringSize;
    pointerMode = usePointerMode;
    writeOff = 0;
    produced = 0;
    uint8_t* arm = pru->shared_ram + (pruBaseAddr - SMEM_RING_SMEM_PRU_ADDR);
    ring = (volatile uint64_t*)arm;
    ctrl = (volatile uint32_t*)(arm + ringSize);
    ctrl[0] = pointerMode ? basePru : 0;
    ctrl[1] = pointerMode ? basePru : 0;
    // the firmware polls for the config; base first, then the nonzero size
    volatile uint32_t* rc = (volatile uint32_t*)(pru->data_ram + SMEM_RING_CONFIG_OFFSET);
    rc[0] = basePru;
    rc[1] = ringSize;
}

uint32_t BBBPruSMEMRing::write(const uint8_t* src, uint32_t len) {
    uint32_t readOff = pointerMode ? (ctrl[1] - basePru) : (ctrl[1] % size);
    uint32_t used = (writeOff - readOff + size) % size;
    uint32_t space = size - 64 - used;
    if (len > space) {
        len = space;
    }
    uint32_t total = 0;
    while (total < len) {
        uint32_t n = len - total;
        if (n > (size - writeOff)) {
            n = size - writeOff;
        }
        volatile uint64_t* d = ring + writeOff / 8;
        const uint64_t* s = (const uint64_t*)(src + total);
        for (uint32_t i = 0; i < n / 8; i++) {
            d[i] = s[i];
        }
        writeOff += n;
        if (writeOff == size) {
            writeOff = 0;
        }
        produced += n;
        total += n;
    }
    if (total) {
        // the data must be observable before the position update
        __sync_synchronize();
        ctrl[0] = pointerMode ? (basePru + writeOff) : produced;
    }
    return total;
}
