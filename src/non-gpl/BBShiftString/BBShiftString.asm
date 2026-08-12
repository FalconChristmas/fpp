/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */
 
 /* 
 * WS281x LED strip driver for the BeagleBone Black.
 *
 * Drives up to 64 strings using the PRU hardware.  The ARM writes
 * rendered frames into shared DDR memory and sets a flag to indicate
 * how many pixels wide the image is.  The PRU then bit bangs the signal
 * out the pru pins and sets a done flag.
 *
 * To stop, the ARM can write a 0xFF to the command, which will
 * cause the PRU code to exit.
 *
 * At 800 KHz:
 *  0 is 0.25 usec high, 1 usec low
 *  1 is 0.60 usec high, 0.65 usec low
 *  Reset is 300 usec
 */

#include "ResourceTable.asm"



// T0 is the instant the zero bits are dropped, and every port on the PRU
// shares it - there is only one latch here - so it has to suit each protocol
// the cape can be carrying at once.
//
// 320ns is the value that does: the TM18xx parts are the tight constraint,
// wanting a 310-410ns low time for a zero code (TM1814 datasheet, typical
// 360), while the ws281x family is far looser - WS2811 100-400, SK6812 and
// WS2815 150-450, WS2812B 250-550.  The overlap is 310-400 and 320 sits in
// it with margin at the end that matters.
//
// This used to be 220, which was under spec for both TM18xx and WS2812B and
// only worked because real silicon is more forgiving than its datasheet.  It
// was also below the work already needed to get here: the 8 deep data shift
// is 33 cycles plus the latch, so on the AM335x the wait was a no-op and the
// real edge landed near 230ns.  A 16 deep shift is 65 cycles (260ns at the
// AM62x 250MHz) and needed its own 300ns value for the same reason; 320
// clears both, so one constant now serves every build.
#define T0_TIME   320
#define T1_TIME   750
#define LOW_TIME  1120

// Runtime bit timing.  The three waits above are the compiled defaults; when
// the ARM publishes cycle counts at the offsets below they take over, so one
// firmware can drive a different bit cell (a 400kHz part, say) without a
// separate image.  See publishTiming() in BBShiftString.cpp.
//
// They live in the "buffer" words of the PRU data RAM struct - unused, and
// crucially in the low 256 bytes, because LBCO's offset field is 8 bit and an
// offset of 256 will not assemble.  That is also why the clock/latch config
// at PINCFG_OFFSET has to be read through a register: fine once per frame in
// the idle loop, too expensive three times per bit.
//
// The ARM writes cycles already reduced by the macro's own overhead, so the
// PRU does no arithmetic and no ns conversion.  A zero means "not published",
// and the compiled default is used instead.
#define TIMING_T0_OFFSET   16
#define TIMING_T1_OFFSET   20
#define TIMING_LOW_OFFSET  24
#define TIMING_MAGIC_OFFSET 28
//if LOW_TIME needs to be more than 1250, you need to do:
// #define SLOW_WAITNS

// To find out whether the ARM pump is keeping the ring fed, uncomment this and
// rebuild.  The firmware then counts every poll that finds the ring empty into
// SMEM_RING_STATS_OFFSET, which the ARM can read from the owning PRU's data RAM.
// This is the only trustworthy measurement of starvation: sampling the read
// pointer from userspace cannot resolve a stall once the output duty cycle gets
// high, because the sampling process's own scheduling jitter runs to about a
// millisecond - the same size as the stalls being looked for.
// Read it as a delta over a known interval.  To convert polls to time, freeze
// the pump (SIGSTOP fppd) mid frame for a known wall time and divide: that
// calibrated one poll at 48.1ns on an AM62x at 250MHz.
// #define RING_STALL_STATS

// Measure where the bit cell edges actually land.  With this defined the
// firmware records the PRU cycle counter at the T0 and T1 latches of every
// bit into the words below, which the ARM can read back: that is the achieved
// timing, including the wait macro's own overhead, rather than the requested
// value.  Use it to calibrate TIMING_OVERHEAD_CYCLES and to check whether a
// window is work bound (achieved > requested means the wait never ran).
// Costs two instructions per edge, so leave it off in shipping firmware.
// It reuses the timing marker word, which is only read once at startup and is
// free afterwards - the other low words are all live (0/4 are the DDR
// addresses, 8 the command, 12 the response).  T1 uses the same macro as T0,
// so calibrating T0 calibrates both.
// #define TIMING_PROBE
#define PROBE_T0_OFFSET TIMING_MAGIC_OFFSET

#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU1
#endif

// Pin positions within r30.  The clock and latch positions are only the
// power-on defaults: the ARM resolves the cape's pin names to r30 bit
// numbers and publishes them at PINCFG_OFFSET (see BBShiftString.cpp), so
// one firmware serves any pinout on a platform.  The data byte and the
// (rarely used) enable pin remain compile time; both can still be
// overridden from the build command line if a pinout ever forces it.
#ifdef AM33XX
#ifndef CONTROL_BYTE
#define CONTROL_BYTE r30.b1
#endif
#ifndef DATA_BYTE
#define DATA_BYTE r30.b0
#endif
#ifdef RUNNING_ON_PRU1
#ifndef CLOCK_PIN
#define CLOCK_PIN 2
#endif
#ifndef LATCH_PIN
#define LATCH_PIN 3
#endif
#ifndef ENABLE_PIN
#define ENABLE_PIN 0
#endif
#else
#ifndef CLOCK_PIN
#define CLOCK_PIN 7
#endif
#ifndef LATCH_PIN
#define LATCH_PIN 6
#endif
#ifndef ENABLE_PIN
#define ENABLE_PIN 0
#endif
#endif
#else
#ifndef CONTROL_BYTE
#define CONTROL_BYTE r30.b2
#endif
#ifndef DATA_BYTE
#define DATA_BYTE r30.b0
#endif
#ifndef CLOCK_PIN
#define CLOCK_PIN 3
#endif
#ifndef LATCH_PIN
#define LATCH_PIN 0
#endif
#ifndef ENABLE_PIN
#define ENABLE_PIN 2
#endif
#endif

// numeric r30 bit of CONTROL_BYTE's bit 0, to convert the byte-relative
// defaults above into the full-register bit numbers the toggles use
#ifndef CONTROL_BIT_BASE
#ifdef AM33XX
#define CONTROL_BIT_BASE 8
#else
#define CONTROL_BIT_BASE 16
#endif
#endif

// Runtime clock/latch positions: full r30 bit numbers, published by the ARM
// as a u32 at PINCFG_OFFSET in our data RAM ({clock, latch, 0, 0xA5}); the
// 0xA5 marker distinguishes a real config from cleared memory.  Loaded in
// the command wait loop, so a value written any time before the first frame
// command takes effect.  r29.b2/b3 are free (cur_data only uses r29.w0).
#define PINCFG_OFFSET 0x1FE8
#define clockBit r29.b2
#define latchBit r29.b3

ENABLE_SEND .macro
    SET CONTROL_BYTE, CONTROL_BYTE, ENABLE_PIN
    .endm

DISABLE_SEND .macro
    CLR CONTROL_BYTE, CONTROL_BYTE, ENABLE_PIN
    .endm

#define FALCONV5_PERIOD  1130


#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"
#include "SMEMRing.hp"

/** Register map */

//16 registers for channel data
// r2 - r17
#define pixelData    r2

#define tmpReg1         r19
#define tmpReg2         r20

#ifdef SHIFT16
// 16 strings per pin needs 16 bytes of high masks and 16 of low, which will
// not fit alongside the 64 byte pixel block: r2-r29 are all spoken for.  Keep
// them in the PRU scratchpad instead - bank 11 high, bank 12 low - and use
// r21-r24 as a transient window.  XIN/XOUT costs one cycle regardless of
// length (measured on the AM62x), so this buys the 32 bytes for two cycles
// per shift phase and no registers at all.
//
// Both banks are free in every supported configuration: bank 10 is the
// FalconV5 listener handshake, and a shared panels+strings cape forces the
// panel driver into SINGLEPRU, whose build has no XIN/XOUT compiled in at all.
#define BYTES_FOR_MASKS 34
#define MASK_HI_BANK 11
#define MASK_LOW_BANK 12
#define OUTPUT_MASKS r21
#define MASK_OVERFLOW r25.w0
#else
//r21-r24 contains the output masks, r21/22 are high, r23/24 are low
// read two extra bytes for the "next"
#define BYTES_FOR_MASKS 18
#define OUTPUT_MASKS r21
#define OUTPUT_HI_MASKS r21
#define OUTPUT_LOW_MASKS r23
#define MASK_OVERFLOW r25.w0
#endif

#define next_check  r25.w0
#define curCommand  r25.w2

#define data_addr	r26
#define fv5_data_addr r27
#define commandReg  r28
#define data_len    r28.w0

// data_flags:
//    t0 :  ignore output table
//    t1 :  send falcon receiver packet
//    t2 :  send a second receiver packet
//    t4 :  enable listen and wait after packet
#define data_flags  r28.w2
#define cur_data	r29.w0

#define DATABLOCKSIZE 64
#define COMMANDTABLE_OFFSET 32

#ifdef AM33XX
PRELOAD_DATA .macro dataAddress
    .endm

LOAD_NEXT_DATABLOCK .macro dataAddress
    LBBO    &pixelData, dataAddress, 0, DATABLOCKSIZE
    ADD     dataAddress, dataAddress, DATABLOCKSIZE
    .endm

UNPRELOAD_DATA .macro dataAddress
    .endm

#else

// AM62x: the data is streamed into a ring in the PRU shared RAM by an ARM
// real-time pump thread (see SMEMRing.hp and BBShiftString.cpp).  DDR reads
// from the PRU (XFR2VBUS or LBBO) have a 1.1-1.8us round trip on the AM62x
// with occasional much larger spikes, which can stretch the WS281x low time
// far enough to latch a partial frame.  Shared RAM reads are ~20 cycles and
// fully predictable.
//
// The ARM streams the frame data followed by any FalconV5 packet data in
// exactly the order the code consumes blocks, so the dataAddress arguments
// are ignored.  The ARM publishes its write pointer (a PRU address, always
// 64-byte aligned) at ring end + 0; the read pointer is published at ring
// end + 4.  Pointer equality means the ring is empty; the ARM never fills
// the last block so the pointers are only equal when empty.  ringCtrl is
// also the first address past the ring for the wrap compare.

#define ringReadPtr     r0
#define ringCtrl        r18

PRELOAD_DATA .macro dataAddress
    .endm

LOAD_NEXT_DATABLOCK .macro dataAddress
    .newblock
#ifdef RING_STALL_STATS
    // Count the polls that find the ring empty (the slot SMEMRing.hp reserves
    // for exactly this).  When data is ready this costs the same two
    // instructions as the plain loop below - LBBO then a branch - so the only
    // overhead lands on the path where we are already starving.  See
    // RING_STALL_STATS at the top of this file for how to read it.
WAITDATA?:
    LBBO    &tmpReg1, ringCtrl, 0, 4
    QBNE    GOTDATA?, tmpReg1, ringReadPtr
    LDI     tmpReg2, SMEM_RING_STATS_OFFSET
    LBCO    &tmpReg1, CONST_PRUDRAM, tmpReg2, 4
    ADD     tmpReg1, tmpReg1, 1
    SBCO    &tmpReg1, CONST_PRUDRAM, tmpReg2, 4
    JMP     WAITDATA?
GOTDATA?:
#else
WAITDATA?:
    LBBO    &tmpReg1, ringCtrl, 0, 4
    QBEQ    WAITDATA?, tmpReg1, ringReadPtr
#endif
    LBBO    &pixelData, ringReadPtr, 0, DATABLOCKSIZE
    ADD     ringReadPtr, ringReadPtr, DATABLOCKSIZE
    // ringCtrl is also the first address past the ring.  Wrap BEFORE
    // publishing so the pointer the ARM sees is always a real ring address -
    // publishing the one-past-the-end value makes an empty ring look occupied
    // until the next block is consumed.
    QBNE    NOWRAP?, ringReadPtr, ringCtrl
    LDI     tmpReg1, SMEM_RING_CONFIG_OFFSET
    LBCO    &ringReadPtr, CONST_PRUDRAM, tmpReg1, 4
NOWRAP?:
    SBBO    &ringReadPtr, ringCtrl, 4, 4
    .endm

UNPRELOAD_DATA .macro dataAddress
    .endm
#endif


// Wait until the PRU cycle counter reaches a target held in data RAM, rather
// than a compile time immediate.  Same shape as WAITNS_LOOP - the MAX clamps
// a target already passed to zero so the LOOP count is never negative - but
// the target arrives as cycles from the ARM.
//
// One extra cost over the immediate form: LBCO from local data RAM is slower
// than LDI, which is why the ARM subtracts TIMING_OVERHEAD_CYCLES rather than
// the 3 the immediate version compensates for.
WAITCYCLES_DRAM .macro dramOff, treg1, treg2
    .newblock
    LDI32 treg1, PRU_CONTROL_REG
    LBBO &treg2, treg1, 0xC, 4
    LBCO &treg1, CONST_PRUDRAM, dramOff, 4
    MAX  treg1, treg2, treg1
    SUB  treg1, treg1, treg2
    LOOP endloop?, treg1
        NOP
endloop?:
    .endm

TOGGLE_CLOCK .macro
    SET r30, r30, clockBit
    NOP
    CLR r30, r30, clockBit
    //NOP
    .endm

TOGGLE_LATCH .macro
    SET r30, r30, latchBit
    NOP
    CLR r30, r30, latchBit
    NOP
    .endm

// strings per data pin, i.e. how deep the cape's shift register chain is
#ifdef SHIFT16
#define SHIFT_DEPTH 16
#else
#define SHIFT_DEPTH 8
#endif

OUTPUT_REG_INDIRECT .macro
    .newblock
    LOOP ENDLOOP?, SHIFT_DEPTH
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_CLOCK
ENDLOOP?:
    .endm

OUTPUT_HIGH .macro
    .newblock
    MOV r1.b1, r1.b0
#ifdef SHIFT16
    XIN MASK_HI_BANK, &OUTPUT_MASKS, 16
    LDI r1.b0, &OUTPUT_MASKS
#else
    LDI r1.b0, &OUTPUT_HI_MASKS
#endif
    OUTPUT_REG_INDIRECT
    MOV r1.b0, r1.b1
    .endm

OUTPUT_LOW .macro
    .newblock
    MOV r1.b1, r1.b0
#ifdef SHIFT16
    XIN MASK_LOW_BANK, &OUTPUT_MASKS, 16
    LDI r1.b0, &OUTPUT_MASKS
#else
    LDI r1.b0, &OUTPUT_LOW_MASKS
#endif
    OUTPUT_REG_INDIRECT
    MOV r1.b0, r1.b1
    .endm

#ifdef SHIFT16
// Pull one 34 byte command table record into the scratchpad: 16 bytes of high
// masks to bank 11, 16 of low to bank 12, then the two byte "next check"
// value.  That last load targets MASK_OVERFLOW, which is the same register
// field as next_check, exactly as the 8 deep single LBCO does.  Advances the
// offset register to the following record.
LOAD_MASKS .macro cmdOffset
    LBCO &OUTPUT_MASKS, CONST_PRUDRAM, cmdOffset, 16
    XOUT MASK_HI_BANK, &OUTPUT_MASKS, 16
    ADD  cmdOffset, cmdOffset, 16
    LBCO &OUTPUT_MASKS, CONST_PRUDRAM, cmdOffset, 16
    XOUT MASK_LOW_BANK, &OUTPUT_MASKS, 16
    ADD  cmdOffset, cmdOffset, 16
    LBCO &MASK_OVERFLOW, CONST_PRUDRAM, cmdOffset, 2
    ADD  cmdOffset, cmdOffset, 2
    .endm
#endif


OUTPUT_FALCONV5_PACKET .macro
    .newblock
    QBBC  DONE_FALCONV5?, data_flags, 1

START_FALCONV5?:
    OUTPUT_LOW
    TOGGLE_LATCH
	SLEEPNS	15300, tmpReg1, 0

    OUTPUT_HIGH
    TOGGLE_LATCH
    PRELOAD_DATA  fv5_data_addr
	SLEEPNS	53500, tmpReg1, 0

FALCONV5_SETUP_LOOP?:
    LOAD_NEXT_DATABLOCK fv5_data_addr
    LDI  data_len, 56
FALCONV5_LOOP?:
    // start bit
    OUTPUT_LOW
    WAITNS_LOOP FALCONV5_PERIOD, tmpReg1, tmpReg2
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    TOGGLE_LATCH
    LDI r1.b0, &pixelData
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
#ifdef SHIFT16
    // as in WORD_LOOP, a 64 byte block only carries four of the eight bits
    LOAD_NEXT_DATABLOCK fv5_data_addr
    LDI r1.b0, &pixelData
#endif
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    JAL r1.w2, OUTPUT_FULL_BIT_FV5
    OUTPUT_HIGH //stop bit
    WAITNS_LOOP  FALCONV5_PERIOD, tmpReg1, tmpReg2
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    TOGGLE_LATCH
    QBEQ DONE_FALCONV5_LOOP?, data_len, 0
        LOAD_NEXT_DATABLOCK fv5_data_addr
        SUB     data_len, data_len, 1
        JMP FALCONV5_LOOP?
DONE_FALCONV5_LOOP?:
    QBBC  DO_FALCONV5_LISTNER?, data_flags, 2
    CLR data_flags, data_flags, 2
    OUTPUT_HIGH
    TOGGLE_LATCH
    SLEEPNS 70000, tmpReg1, 0
    JMP FALCONV5_SETUP_LOOP?

DO_FALCONV5_LISTNER?:
    UNPRELOAD_DATA data_addr
    QBBC  DONE_FALCONV5?, data_flags, 3
    OUTPUT_LOW
    TOGGLE_LATCH
    SLEEPNS	15500, tmpReg1, 0
    OUTPUT_HIGH
    TOGGLE_LATCH
    SLEEPNS	19500, tmpReg1, 0

    DISABLE_SEND
    SLEEPNS	 500, tmpReg1, 0

    LDI tmpReg1, 1
    LDI tmpReg2, 0
    XOUT 10, &tmpReg1, 8
    SLEEPNS	500000, tmpReg1, 0
    XIN 10, &tmpReg2, 4
    QBEQ NO_DATA_FOUND, tmpReg2, 0
    SLEEPNS	500000, tmpReg1, 0
    SLEEPNS	500000, tmpReg1, 0
    SLEEPNS	500000, tmpReg1, 0
    SLEEPNS	100000, tmpReg1, 0
NO_DATA_FOUND:
    LDI tmpReg1, 0
    XOUT 10, &tmpReg1, 4
    SLEEPNS	10000, tmpReg1, 0
    ENABLE_SEND

DONE_FALCONV5?:
    .endm

#if !defined(FIRST_CHECK)
#define FIRST_CHECK NO_PIXELS_CHECK
#endif

;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
    .sect    ".text:main"
    .global    ||main||
||main||:
#ifdef AM33XX
	// Enable OCP master port
	// clear the STANDBY_INIT bit in the SYSCFG register,
	// otherwise the PRU will not be able to write outside the
	// PRU memory space and to the BeagleBon's pins.
	LBCO	&r0, C4, 4, 4
	CLR	    r0, r0, 4
	SBCO	&r0, C4, 4, 4
#endif    

	// Configure the programmable pointer register for PRU by setting
	// c28_pointer[15:0] field to 0x0120.  This will make C28 point to
	// 0x00012000 (PRU shared RAM).
	//MOV	r0, 0x00000120
	//MOV	r1, CTPPR_0 + PRU_MEMORY_OFFSET
	//SBBO    &r0, r1, 0x00, 4

	// Configure the programmable pointer register for PRU by setting
	// c31_pointer[15:0] field to 0x0010.  This will make C31 point to
	// 0x80001000 (DDR memory).
	LDI32	r0, 0x00100000
	LDI32	r1, CTPPR_1 + PRU_MEMORY_OFFSET
    SBBO    &r0, r1, 0x00, 4

#ifdef SHIFT16
    // The scratchpad mask transfers assume XIN/XOUT is register aligned.  A
    // panel driver sharing this PRUSS turns on the PRUSS wide XIN/XOUT shift
    // feature (SPP bit 0), which displaces every transfer by r0.b0.  Our r0 is
    // the 64 byte aligned ring read pointer and the shift amount is only the
    // low 5 bits, so it masks to zero either way - but do not leave that a
    // coincidence.  Clear just bit 0: the rest of SPP is PRU1's high priority
    // OCP port enable, which the panel driver does want.  Both of these have
    // to happen before r0 becomes the ring pointer below.
    LDI32   r1, PRU_CONFIG_REG
    LBBO    &r0, r1, 0x34, 4
    CLR     r0, r0, 0
    SBBO    &r0, r1, 0x34, 4
#endif

#ifndef AM33XX
    // Wait for the ARM to publish the ring location/size into our data RAM
    // (see SMEMRing.hp).  It cannot be written before the firmware starts:
    // the firmware load clears the PRU memories and writes while the PRUSS
    // is powered down are silently dropped.  The base is written first,
    // then the (nonzero) size.
    LDI     tmpReg1, SMEM_RING_CONFIG_OFFSET
RINGCFGWAIT:
    LBCO    &ringReadPtr, CONST_PRUDRAM, tmpReg1, 8   // base into r0, size into r1
    QBEQ    RINGCFGWAIT, r1, 0
    ADD     ringCtrl, ringReadPtr, r1
    // publish the read pointer so the ARM pump starts consistent
    SBBO    &ringReadPtr, ringCtrl, 4, 4
#endif

    // Wait for the ARM to publish the bit timing (see publishTiming() in
    // BBShiftString.cpp).  Like the ring config it cannot be written before
    // the firmware starts, because loading the firmware clears these memories.
    // The ARM writes the three cycle counts first and the marker last, so
    // seeing the marker means all three are good.
TIMINGWAIT:
    LBCO    &tmpReg1, CONST_PRUDRAM, TIMING_MAGIC_OFFSET, 4
    LDI32   tmpReg2, 0xA5A5A5A5
    QBNE    TIMINGWAIT, tmpReg1, tmpReg2

	// Write a 0x1 into the response field so that they know we have started
	LDI 	r2, 0x1
	SBCO	&r2, CONST_PRUDRAM, 8, 4

    // clock/latch default to the compiled positions until the ARM publishes
    // the resolved ones
    LDI     clockBit, (CONTROL_BIT_BASE + CLOCK_PIN)
    LDI     latchBit, (CONTROL_BIT_BASE + LATCH_PIN)

    // Make sure the data address and command are cleared at start.  The
    // command word has to be cleared explicitly: the 0x1 written above is a
    // startup marker, and leaving it there makes the wait loop below read it
    // back as a one byte frame and consume a ring block the ARM never wrote.
	LDI 	r1, 0x0
	LDI 	r2, 0x0
	SBCO	&r1, CONST_PRUDRAM, 0, 8
	SBCO	&r1, CONST_PRUDRAM, 8, 4

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    //make sure the clock starts
    RESET_PRU_CLOCK tmpReg1, tmpReg2

    // pick up the runtime clock/latch bit numbers if the ARM has published
    // them (reread while idle so they always apply before the first frame)
    LDI     tmpReg1, PINCFG_OFFSET
    LBCO    &tmpReg2, CONST_PRUDRAM, tmpReg1, 4
    QBNE    NOPINCFG, tmpReg2.b3, 0xA5
    MOV     clockBit, tmpReg2.b0
    MOV     latchBit, tmpReg2.b1
NOPINCFG:

	// Load the pointer to the buffer from PRU DRAM into data_addr, packet addr and the
	// start command into commandReg
	LBCO	&data_addr, CONST_PRUDRAM, 0, 12

	// Wait for a non-zero command
	QBEQ	_LOOP, commandReg, 0

	// Command of 0xFFFF is the signal to exit
    LDI     tmpReg1, 0xFFFF
	QBNE	CONT_DATA, commandReg, tmpReg1
    JMP EXIT

CONT_DATA:
    PRELOAD_DATA  data_addr
    ENABLE_SEND

    // reset command to 0 so ARM side will send more data
    LDI     r1, 0
    SBCO    &r1, CONST_PRUDRAM, 8, 4

    // Reset the output masks
#ifdef SHIFT16
    // LOAD_MASKS walks curCommand past the record it just read, so it ends up
    // pointing at the next one - the same place the 8 deep LDI below puts it.
    LDI     curCommand, COMMANDTABLE_OFFSET
    LOAD_MASKS curCommand
#else
    LBCO	&OUTPUT_MASKS, CONST_PRUDRAM, COMMANDTABLE_OFFSET, BYTES_FOR_MASKS
#endif
    // reset the command table
    MOV next_check, MASK_OVERFLOW
    QBBC NO_CUSTOM_CHECKS, data_flags, 0
        MOV next_check, data_len
NO_CUSTOM_CHECKS:
#ifndef SHIFT16
    LDI curCommand, COMMANDTABLE_OFFSET + BYTES_FOR_MASKS
#endif
	LDI	cur_data, 1

    //start the clock
    RESET_PRU_CLOCK tmpReg1, tmpReg2

WORD_LOOP:
    LOAD_NEXT_DATABLOCK data_addr
    LDI r1.b0, &pixelData
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
#ifdef SHIFT16
    // each bit consumes 16 bytes at 16 strings per pin, so a 64 byte block is
    // only four bits deep and one pixel byte needs two of them
    LOAD_NEXT_DATABLOCK data_addr
    LDI r1.b0, &pixelData
#endif
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    QBNE NO_COMMAND_NEEDED, cur_data, next_check
#ifdef SHIFT16
        LOAD_MASKS curCommand
#else
        LBCO &OUTPUT_MASKS, CONST_PRUDRAM, curCommand, BYTES_FOR_MASKS
        ADD curCommand, curCommand, BYTES_FOR_MASKS
#endif
        MOV next_check, MASK_OVERFLOW
NO_COMMAND_NEEDED:
    ADD cur_data, cur_data, 1
    QBGT WORD_LOOP_DONE, data_len, cur_data
    JMP WORD_LOOP

WORD_LOOP_DONE:
    UNPRELOAD_DATA data_addr
    NOP 
    NOP 

    OUTPUT_FALCONV5_PACKET
    NOP
    NOP
    OUTPUT_LOW
    NOP
    NOP
    TOGGLE_LATCH

    // Delay at least 300 usec; this is the required reset
	// time for the LED strip to update with the new pixels.
	SLEEPNS	300000, tmpReg1, 0

	// Go back to waiting for the next frame buffer
	JMP	_LOOP


EXIT:
	// Write a 0xFFFF into the response field so that they know we're done
	LDI r2, 0xFFFF
	SBCO &r2, CONST_PRUDRAM, 12, 4

	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT


OUTPUT_FULL_BIT:
    OUTPUT_HIGH
    //wait for the full cycle to complete
    WAITCYCLES_DRAM  TIMING_LOW_OFFSET, tmpReg1, tmpReg2
    //start the clock
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    TOGGLE_LATCH
    OUTPUT_REG_INDIRECT
    WAITCYCLES_DRAM  TIMING_T0_OFFSET, tmpReg1, tmpReg2
    TOGGLE_LATCH
#ifdef TIMING_PROBE
    GET_PRU_CLOCK tmpReg1, tmpReg2, 4
    SBCO &tmpReg1, CONST_PRUDRAM, PROBE_T0_OFFSET, 4
#endif
    OUTPUT_LOW
    WAITCYCLES_DRAM  TIMING_T1_OFFSET, tmpReg1, tmpReg2
    TOGGLE_LATCH
    JMP r1.w2

OUTPUT_FULL_BIT_FV5:
    OUTPUT_REG_INDIRECT
    WAITNS_LOOP  FALCONV5_PERIOD, tmpReg1, tmpReg2
    TOGGLE_LATCH
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    JMP r1.w2
