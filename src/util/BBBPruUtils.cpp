#include "fpp-pch.h"

#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <elf.h>
#include <filesystem>

#include "BBBPruUtils.h"


static unsigned int proc_read(const char * const fname) {
    FILE * const f = fopen(fname, "r");
    unsigned int x;
    fscanf(f, "%x", &x);
    fclose(f);
    return x;
}

static bool hasUIO = true;


class PRUCoreInfo {
public:
    int pru_num = 0;
    uint8_t *dataRam = nullptr;
    uintptr_t dataRamPhyLoc = 0;
    int dataRamSize = 0;
    uint8_t *sharedRam = nullptr;
    uint32_t sharedRamPhyLoc = 0;
    int sharedRamSize = 0;

    uint8_t *instructionRam = nullptr;
    int instructionRamSize = 0;

    unsigned int *controlRegs = nullptr;

    void disable() {
        if (!hasUIO) {
            std::string filename = "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
            if (FileExists(filename)) {
                FILE *rp = fopen(filename.c_str(), "w");
                fprintf(rp, "stop");
                fclose(rp);
            }
        }
        controlRegs[0] = 1;
    }
    void enable(uint32_t addr = 0) {
        if (!hasUIO) {
            std::string filename = "/sys/class/remoteproc/remoteproc" + std::to_string(pru_num) + "/state";
            int cnt = 0;
            while (!FileExists(filename) && cnt < 20000) {
                cnt++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (FileExists(filename) && cnt < 20000) {
                FILE *rp = fopen(filename.c_str(), "w");
                fprintf(rp, "start");
                fclose(rp);
            } else {
                LogWarn(VB_CHANNELOUT, "BBBPru::Pru::enable() - could not start PRU core %d\n", pru_num);
            }
        } else {
            uint32_t a = addr;
            a /= sizeof(uint32_t);
            a <<= 16;
            a |= 2;
            controlRegs[0] = a;
        }
    }
};


static int prussUseCount = 0;

static uint8_t *ddr_mem_loc = nullptr;
static uint32_t ddr_phy_mem_loc = 0;
static size_t ddr_filelen = 0;
static uint8_t *base_memory_location = nullptr;
static PRUCoreInfo prus[2];

#define AM33XX_PRU_BASE 0x4a300000
#define AM33XX_PRUSS_MMAP_SIZE               0x40000

#define AM33XX_DATARAM0_PHYS_BASE            0x4a300000
#define AM33XX_DATARAM1_PHYS_BASE            0x4a302000
#define AM33XX_INTC_PHYS_BASE                0x4a320000
#define AM33XX_PRU0CONTROL_PHYS_BASE         0x4a322000
#define AM33XX_PRU0DEBUG_PHYS_BASE           0x4a322400
#define AM33XX_PRU1CONTROL_PHYS_BASE         0x4a324000
#define AM33XX_PRU1DEBUG_PHYS_BASE           0x4a324400
#define AM33XX_PRU0IRAM_PHYS_BASE            0x4a334000
#define AM33XX_PRU1IRAM_PHYS_BASE            0x4a338000
#define AM33XX_PRUSS_SHAREDRAM_BASE          0x4a310000
#define AM33XX_PRUSS_CFG_BASE                0x4a326000
#define AM33XX_PRUSS_UART_BASE               0x4a328000
#define AM33XX_PRUSS_IEP_BASE                0x4a32e000
#define AM33XX_PRUSS_ECAP_BASE               0x4a330000
#define AM33XX_PRUSS_MIIRT_BASE              0x4a332000
#define AM33XX_PRUSS_MDIO_BASE               0x4a332400


static void initPrus() {
    const int mem_fd = open("/dev/mem", O_RDWR);
    
    base_memory_location = (uint8_t*)mmap(0,
                                          AM33XX_PRUSS_MMAP_SIZE,
                                         PROT_WRITE | PROT_READ,
                                          MAP_SHARED,
                                          mem_fd,
                                          AM33XX_PRU_BASE);
    
    uintptr_t ddr_addr = 0x8f000000;
    uintptr_t ddr_sizeb = 0x00400000;

    if (FileExists("/sys/class/uio/uio0/maps/map1/addr")) {
        hasUIO = true;
        ddr_addr = proc_read("/sys/class/uio/uio0/maps/map1/addr");
        ddr_sizeb = proc_read("/sys/class/uio/uio0/maps/map1/size");
    } else {
        hasUIO = false;
        if (!FileExists("/sys/class/remoteproc/remoteproc0/state")) {
            system("modprobe pru_rproc");
        }
    }
    if (ddr_mem_loc == nullptr) {
        ddr_phy_mem_loc = ddr_addr;
        ddr_filelen = ddr_sizeb;
        ddr_mem_loc = (uint8_t*)mmap(0,
                                     ddr_filelen,
                                     PROT_WRITE | PROT_READ,
                                     MAP_SHARED,
                                     mem_fd,
                                     ddr_addr);
    }
    close(mem_fd);

    prus[0].pru_num = 0;
    prus[1].pru_num = 1;
    prus[0].controlRegs = (uint32_t*)(base_memory_location + (AM33XX_PRU0CONTROL_PHYS_BASE - AM33XX_PRU_BASE));
    prus[1].controlRegs = (uint32_t*)(base_memory_location + (AM33XX_PRU1CONTROL_PHYS_BASE - AM33XX_PRU_BASE));
    prus[0].dataRam = base_memory_location + (AM33XX_DATARAM0_PHYS_BASE - AM33XX_PRU_BASE);
    prus[0].dataRamSize = 8*1024;
    prus[0].dataRamPhyLoc = AM33XX_DATARAM0_PHYS_BASE;
    prus[1].dataRam = base_memory_location + (AM33XX_DATARAM1_PHYS_BASE - AM33XX_PRU_BASE);
    prus[1].dataRamSize = 8*1024;
    prus[1].dataRamPhyLoc = AM33XX_DATARAM1_PHYS_BASE;
    prus[0].sharedRam = base_memory_location + (AM33XX_PRUSS_SHAREDRAM_BASE - AM33XX_PRU_BASE);
    prus[0].sharedRamSize = 12*1024;
    prus[0].sharedRamPhyLoc = AM33XX_PRUSS_SHAREDRAM_BASE;
    prus[1].sharedRam = base_memory_location + (AM33XX_PRUSS_SHAREDRAM_BASE - AM33XX_PRU_BASE);
    prus[1].sharedRamSize = 12*1024;
    prus[1].sharedRamPhyLoc = AM33XX_PRUSS_SHAREDRAM_BASE;
    prus[0].instructionRam = base_memory_location + (AM33XX_PRU0IRAM_PHYS_BASE - AM33XX_PRU_BASE);
    prus[0].instructionRamSize = 8*1024;
    prus[1].instructionRam = base_memory_location + (AM33XX_PRU1IRAM_PHYS_BASE - AM33XX_PRU_BASE);
    prus[1].instructionRamSize = 8*1024;
}

BBBPru::BBBPru(int pru, bool mapShared, bool mapOther) : pru_num(pru) {
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
            munmap(base_memory_location, AM33XX_PRUSS_MMAP_SIZE);
            base_memory_location = nullptr;
        }
    }
}

int BBBPru::run(const std::string &program) {
    LogDebug(VB_CHANNELOUT, "BBBPru::run(%s)\n", program.c_str());
    
    if (!hasUIO) {
        LogDebug(VB_CHANNELOUT, "Using remoteproc: %s\n", program.c_str());
        prus[pru_num].disable();
        std::filesystem::copy(program, "/lib/firmware/am335x-pru" + std::to_string(pru_num) + "-fw", std::filesystem::copy_options::overwrite_existing);
        prus[pru_num].enable();
        return false;
    }
    
    struct stat st;
    stat(program.c_str(), &st);
    size_t file_size = st.st_size;
    uint32_t startOffset = 0;
    int fhand = open(program.c_str(), O_RDWR);
    uint8_t* buf = (uint8_t*)mmap(0, file_size, PROT_READ, MAP_FILE|MAP_PRIVATE, fhand, 0);
    close(fhand);

    prus[pru_num].disable();
    if (buf[0] == ELFMAG0 && buf[1] == ELFMAG1 && buf[2] == ELFMAG2 && buf[3] == ELFMAG3) {
        //elf linked unit
        LogDebug(VB_CHANNELOUT, "Elf: %s\n", program.c_str());

        if (buf[EI_CLASS] == ELFCLASS32) {
            Elf32_Ehdr *hdr = (Elf32_Ehdr*)buf;
            uint32_t saddress = hdr->e_entry;
            Elf32_Phdr *phdr = (Elf32_Phdr*) &buf[hdr->e_phoff];
            for (int pn = 0; pn < hdr->e_phnum; pn++, phdr++) {
                if (phdr->p_type == PT_LOAD && (phdr->p_flags & PF_X)) {
                    //printf("Type: %d     VA: %X    OFF: %X   SZ: %d\n", phdr->p_type, phdr->p_vaddr, phdr->p_offset, phdr->p_memsz);
                    startOffset = phdr->p_vaddr;
                    uint32_t *tt2 = (uint32_t*)prus[pru_num].instructionRam;
                    for (int x = 0; x < startOffset/4; x++) {
                        //kind of a no-op (LDI r2, 1) to fill space until the real starting point
                        tt2[x] = 0x240001e2;
                    }
                    memcpy(&prus[pru_num].instructionRam[startOffset], &buf[phdr->p_offset], phdr->p_memsz);
                    __asm__ __volatile__("":::"memory");
                }
            }
        } else {
            printf("FIXME for 64bit\n");
        }
    } else {
        LogDebug(VB_CHANNELOUT, "Raw bin: %s\n", program.c_str());
        //raw bin file
        if (file_size < prus[pru_num].instructionRamSize) {
            memcpy(prus[pru_num].instructionRam, buf, file_size);
        }
    }
    munmap(buf, file_size);

    /*
    char bname[50];
    sprintf(bname,"/tmp/pru-%d.bin", pru_num);
    fhand = open(bname, O_CREAT | O_RDWR);
    write(fhand, prus[pru_num].instructionRam, 3000);
    close(fhand);
    */
    
    prus[pru_num].enable(startOffset);
    return false;
}
void BBBPru::stop() {
    prus[pru_num].disable();
}


