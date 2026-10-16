/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

#include "../Warnings.h"
#include "../common_mini.h"
#include "../log.h"
#include "../channeloutput/serialutil.h"

#include "BBBPruSerial.h"
#include "BBBPruUtils.h"
#include "GPIOUtils.h"

#ifdef PLATFORM_BB64

// The PocketBeagle capes this is for run their strings with BBB48String,
// which uses PRU1 on the AM62x and only takes PRU0 for pins on the MCU GPIO
// controller, which no PocketBeagle 2 header pin is.
static constexpr int PRU_NUM = 0;
static constexpr uint32_t PRU_HZ = 250000000;

// Must match PRUSerial.asm.  Word offsets into the PRU data RAM.
static constexpr int CMD = 0x00 / 4;
static constexpr int RESP = 0x04 / 4;
static constexpr int HEAD = 0x08 / 4;
static constexpr int TAIL = 0x0C / 4;
static constexpr int CONFIG = 0x10 / 4;
static constexpr int RING = 0x100 / 4;
static constexpr uint32_t RING_ENTRIES = 1024;
static constexpr uint32_t HOLD_LOW = 0x80000000;
static constexpr uint32_t HOLD_HIGH = 0x40000000;
static constexpr uint32_t HOLD_MAX = 0x3FFFFFFF;

// The OUT register of each GPIO bank, indexed by mappedGPIOIdx(); the set and
// clear registers follow it.  Same table as FalconPRUDefs.hp.
static constexpr uint32_t GPIO_OUT_REG[4] = { 0x04201014, 0x0060003C, 0x00600064, 0x00601014 };

static std::atomic<bool> pruInUse(false);

class BBBPruSerial : public SerialPortBackend {
public:
    BBBPruSerial(BBBPru* p, uint32_t bitCyc, int db, char par, int sb) :
        pru(p),
        ram((volatile uint32_t*)p->data_ram),
        bitCycles(bitCyc),
        dataBits(db),
        parity(par),
        stopBits(sb) {
    }
    ~BBBPruSerial() override {
        ram[CMD] = 0xFF;
        __sync_synchronize();
        for (int i = 0; i < 50 && ram[RESP] != 0xFFFF; i++) {
            usleep(1000);
        }
        // cleared while the core is still up; its data RAM can be unreachable
        // once remoteproc has stopped it
        ram[CMD] = 0;
        pru->stop();
        delete pru;
        pruInUse = false;
    }

    int write(const uint8_t* data, int len, int timeoutMs) override {
        std::lock_guard<std::mutex> l(lock);
        uint64_t deadline = GetMonotonicTimeMS() + timeoutMs;
        int sent = 0;
        while (sent < len) {
            uint32_t room = freeEntries();
            if (room == 0) {
                publish();
                if (GetMonotonicTimeMS() >= deadline) {
                    break;
                }
                usleep(200);
                continue;
            }
            while (room-- && sent < len) {
                push(frame(data[sent++]));
            }
        }
        publish();
        return sent;
    }

    void sendBreak(int us) override {
        std::lock_guard<std::mutex> l(lock);
        uint64_t deadline = GetMonotonicTimeMS() + 100;
        while (freeEntries() < 2 && GetMonotonicTimeMS() < deadline) {
            usleep(200);
        }
        if (freeEntries() < 2) {
            return;
        }
        // the mark after it gives the receiver time to see the line come back
        // up before the next start bit; at least 20us, as a tty break then a
        // usleep(20) gives (DMX needs 8us)
        uint64_t low = (uint64_t)us * (PRU_HZ / 1000000);
        uint64_t mark = std::max<uint64_t>(20 * (PRU_HZ / 1000000), 2 * bitCycles);
        push(HOLD_LOW | (uint32_t)std::min<uint64_t>(low, HOLD_MAX));
        push(HOLD_HIGH | (uint32_t)std::min<uint64_t>(mark, HOLD_MAX));
        publish();
    }

    bool waitDrained(int timeoutMs) override {
        uint64_t deadline = GetMonotonicTimeMS() + timeoutMs;
        while (ram[TAIL] != head) {
            if (GetMonotonicTimeMS() >= deadline) {
                return false;
            }
            usleep(200);
        }
        return true;
    }

private:
    uint32_t freeEntries() const {
        return (ram[TAIL] - head - 1) & (RING_ENTRIES - 1);
    }
    void push(uint32_t entry) {
        ram[RING + head] = entry;
        head = (head + 1) & (RING_ENTRIES - 1);
    }
    void publish() {
        // the entries have to be in place before the PRU sees the new head
        __sync_synchronize();
        ram[HEAD] = head;
    }
    // start bit, data LSB first, parity, stop bits, as one entry
    uint32_t frame(uint8_t b) const {
        uint32_t bits = 0;
        int n = 1; // start bit is the 0 already in bit 0
        uint32_t d = b & ((1u << dataBits) - 1);
        bits |= d << n;
        n += dataBits;
        if (parity == 'E' || parity == 'O') {
            int ones = __builtin_popcount(d);
            uint32_t p = (parity == 'E') ? (ones & 1) : !(ones & 1);
            bits |= p << n;
            n++;
        }
        for (int s = 0; s < stopBits; s++) {
            bits |= 1u << n++;
        }
        return ((uint32_t)n << 16) | bits;
    }

    BBBPru* pru;
    volatile uint32_t* ram;
    uint32_t bitCycles;
    int dataBits;
    char parity;
    int stopBits;
    uint32_t head = 0;
    std::mutex lock;
};

int BBBPruSerialOpen(const PinCapabilities& pin, int baud, const char* mode, const std::string& desc) {
    auto fail = [&](const std::string& why) {
        std::string w = desc + ": cannot send on " + pin.name + " (" + why + ")";
        LogErr(VB_CHANNELOUT, "%s\n", w.c_str());
        // permanent, like the other serial output config errors
        WarningHolder::AddWarning(26, w);
        return -1;
    };
    int dataBits = (mode && mode[0] >= '5' && mode[0] <= '8') ? mode[0] - '0' : 8;
    char parity = (mode && mode[0]) ? mode[1] : 'N';
    int stopBits = (mode && mode[0] && mode[1] && mode[2] == '2') ? 2 : 1;
    if (baud <= 0 || PRU_HZ / baud < 16) {
        return fail("unsupported speed " + std::to_string(baud));
    }
    int gpioIdx = pin.mappedGPIOIdx();
    int bit = pin.mappedGPIO();
    if (gpioIdx < 0 || gpioIdx > 3 || bit < 0 || bit > 31) {
        return fail("no GPIO the PRU can drive");
    }
    if (pruInUse.exchange(true)) {
        return fail("the PRU transmitter is already used by another serial output");
    }

    CompilePRUProgram("/opt/fpp/src/pru/compilePRUSerial.sh", { "-DRUNNING_ON_PRU" + std::to_string(PRU_NUM) });
    BBBPru* pru = nullptr;
    try {
        pru = new BBBPru(PRU_NUM);
    } catch (const std::exception& ex) {
        pruInUse = false;
        return fail(ex.what());
    }
    if (!pru->run("/tmp/PRUSerial.out")) {
        delete pru;
        pruInUse = false;
        return fail("could not start PRU" + std::to_string(PRU_NUM));
    }

    // the line idles high (mark) before the PRU takes it over
    pin.configPin("gpio", true, desc);
    pin.setValue(true);

    uint32_t bitCycles = (PRU_HZ + baud / 2) / baud;
    volatile uint32_t* ram = (volatile uint32_t*)pru->data_ram;
    ram[CONFIG + 0] = GPIO_OUT_REG[gpioIdx] + 4; // set
    ram[CONFIG + 1] = GPIO_OUT_REG[gpioIdx] + 8; // clear
    ram[CONFIG + 2] = 1u << bit;
    ram[CONFIG + 3] = bitCycles;
    ram[HEAD] = 0;
    ram[TAIL] = 0;
    __sync_synchronize();
    ram[CMD] = 1;

    LogInfo(VB_CHANNELOUT, "%s: sending on %s from PRU%d at %d baud %s (%u cycles per bit)\n",
            desc.c_str(), pin.name.c_str(), PRU_NUM, baud, mode, bitCycles);
    return SerialAttachBackend(new BBBPruSerial(pru, bitCycles, dataBits, parity, stopBits));
}

#else

int BBBPruSerialOpen(const PinCapabilities& pin, int baud, const char* mode, const std::string& desc) {
    return -1;
}

#endif
