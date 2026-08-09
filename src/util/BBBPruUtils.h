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

    // clearSharedMems: clear the shared RAM and (if mapped) the other
    // PRU's data RAM after the firmware load.  Pass false when another
    // driver owns the other PRU (a shared panels + strings cape) so a
    // restart of this output cannot wipe its ring or command state.
    bool run(const std::string& program, bool clearSharedMems = true);
    void stop();

    // Halt/restart the core in place.  Unlike stop()/run() these do not go
    // through remoteproc or reload the firmware image: the program counter
    // and every register survive, so resume() continues from exactly where
    // pause() stopped and the data/shared RAM mappings stay usable while the
    // core is halted.
    //
    // Two things to know before pausing:
    //  - the output pins hold whatever the program last drove onto them, so
    //    only pause where the firmware has parked them somewhere safe
    //  - the cycle counter stops, which is usually the point: it saturates
    //    after 2^32 cycles (~17s at 250MHz) and the hardware clears CTR_EN
    //    when it does.  RESET_PRU_CLOCK does not recover from that, so any
    //    firmware spinning on GET_PRU_CLOCK would hang forever afterwards.
    //    A core left running but idle for >17s hits this; a paused one does
    //    not.
    void pause();
    void resume();

    void clearPRUMem(uint8_t* ptr, size_t sz);
    void memcpyToPRU(uint8_t* dst, uint8_t* src, size_t sz);

    unsigned pru_num;

    uint8_t* data_ram;    // PRU data ram in ARM space
    size_t data_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* shared_ram;    // PRU data ram in ARM space
    size_t shared_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* other_data_ram;    // PRU data ram in ARM space
    size_t other_data_ram_size; // size in bytes of the PRU's data RAM

    uint8_t* ddr;      // PRU DMA address (in ARM space)

    // Shared allocator for the PRU reserved DDR region.  Multiple outputs
    // can coexist on it (K8-B-Scroller: BBB48String strings + BBBMatrix
    // panels + BBBSerial) and historically used hardcoded offsets that
    // could silently overlap.  Allocations are page aligned, first fit
    // (partial config reloads free and re-request in arbitrary order), and
    // named so a re-request by the same owner replaces its allocation.
    // Returns nullptr (and logs) when the region cannot fit the request.
    static uint8_t* ddrAlloc(const std::string& owner, size_t size, uint32_t& physAddr, size_t minOffset = 0);
    static void ddrRelease(const std::string& owner);
    uint32_t ddr_addr; // PRU DMA address (in PRU space)
    size_t ddr_size;   // Size in bytes of the shared space

    uint8_t* sram;      // SRAM data ram in ARM space
    uint32_t sram_addr; // SRAM address (in PRU space)
    size_t sram_size;   // size in bytes of the SRAM

    uint8_t* m4ram;      // M4RAM data ram in ARM space
    uint32_t m4ram_addr; // M4RAM address (in PRU space)
    size_t m4ram_size;   // size in bytes of the M4RAM
};

/**
 * ARM-side producer for a ring buffer in the PRU shared RAM, used to stream
 * output data to a PRU program with short, predictable read latency instead
 * of having the PRU read DDR (1.1us+ per read on the AM62x, with much larger
 * spikes).  The layout and the data-RAM config handshake are described in
 * pru/SMEMRing.hp; the firmware polls its data RAM for the ring location, so
 * attach() must be called after BBBPru::run().
 *
 * Two flow-control conventions are supported, chosen by the firmware:
 *  - counters: the control words hold free-running produced/consumed byte
 *    counts (used when the consumer wants to cache its available window)
 *  - pointers: the control words hold the write/read addresses; equality
 *    means empty, which requires all writes to be multiples of the consumer
 *    block size (64 bytes)
 * The ring is never filled completely so full and empty stay unambiguous.
 */
class BBBPruSMEMRing {
public:
    // writes the ring config into the PRU data RAM (the firmware polls for
    // it) and resets the control words; pru must have been created with
    // mapShared=true
    void attach(BBBPru* pru, uint32_t pruBaseAddr, uint32_t ringSize, bool usePointerMode);
    bool attached() const { return ring != nullptr; }

    // copies up to len bytes (multiple of 8, from an 8-byte aligned src)
    // into the ring using aligned 64-bit stores (the mapping is device
    // memory) and publishes the new write position; returns the number of
    // bytes written, limited by the available space
    uint32_t write(const uint8_t* src, uint32_t len);

    // true once the consumer has caught up with every byte written; both
    // conventions publish positions that compare equal only when empty.
    // Producers that stop on a frame boundary can use this to know the PRU
    // has consumed exactly what was produced, so the two stay aligned in the
    // byte stream across a pause.
    // Compared modulo the ring size: a consumer that publishes its read
    // pointer before wrapping reports "one past the end", which is the same
    // position as the base.  Comparing raw would call that not-drained
    // forever and wedge a producer waiting on it.
    bool drained() const {
        if (!ctrl) {
            return false;
        }
        if (!pointerMode) {
            return ctrl[0] == ctrl[1];
        }
        return ((ctrl[0] - basePru) % size) == ((ctrl[1] - basePru) % size);
    }

    uint32_t size = 0;

private:
    volatile uint64_t* ring = nullptr;
    volatile uint32_t* ctrl = nullptr;
    uint32_t basePru = 0;
    uint32_t writeOff = 0;
    uint32_t produced = 0;
    bool pointerMode = false;
};
