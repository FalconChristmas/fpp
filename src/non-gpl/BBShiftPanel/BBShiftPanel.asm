/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */
 

#include "ResourceTable.asm"



#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU1
#endif


#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"
#include "SMEMRing.hp"



#define OE_PIN   9
#define LATCH_PIN 17
#define CLOCK_PIN 10

#define OSHIFT_PIN  8
#define OLATCH_PIN  16
#define OCLR_PIN    18

// data RAM offset of the panel-init parameter block; clear of the
// brightness table (ends ~0x1808) and the SMEM ring config/stats (0x1FE0+).
// Must match BBShiftPanel.cpp.
#define PANEL_INIT_OFFSET 0x1E00
// data RAM offset of the addressing config (u32: b0 = row address type,
// b1 = scan rows, b2 = row shift chain length), written by the ARM after the
// firmware starts, before the ring config handshake.  Must match
// BBShiftPanel.cpp.
#define ADDR_CONFIG_OFFSET 0x1DF8

#define SEL0_PIN    11
#define SEL1_PIN    12
#define SEL2_PIN    13
#define SEL3_PIN    14
#define SEL4_PIN    15

/** Register map */
#define dataOutReg  r1.b0
#define outputData  r2

// SMEM ring buffer (filled by the ARM pump thread, see BBShiftPanel.cpp and
// SMEMRing.hp).  ringCtrl doubles as the ring end address for the wrap
// compare.  ringBase and the size are read from data RAM at startup so the
// same firmware can use the whole shared RAM or share it with a program on
// the other PRU.
#define ringReadPtr r14
#define ringBase    r15
#define availBytes  r16
#define consumedCnt r17
#define ringCtrl    r18

#define curPixel    r24.w0
#define curStride   r24.w2
#define curBright   r25
#define curAddress  r26.b3
#define data_addr	r27
#define numPixels   r28.w0
#define numStrides  r28.w2

#define tmpReg1     r19
#define tmpReg2     r20

// instrumentation: counts iterations spent waiting for the ARM pump thread
// to fill the ring and total frames output; exported to PRU DRAM at
// SMEM_RING_STATS_OFFSET each frame
#define stallCount  r21
#define frameCount  r22

// row addressing config (see ADDR_CONFIG_OFFSET): 0/2 = the address value
// from the stride table goes straight onto the SEL pins; 1/3/4 = the row is
// clocked into an external row-select shift register (see ROW_ADDR_SHIFT)
#define addrMode    r29.b0
#define addrRows    r29.b1
#define addrChain   r29.b2

#define DATA_BYTE   r30.b0

#ifdef SINGLEPRU
DISPLAY_OFF .macro
    SET r30, r30, OE_PIN
    .endm

DISPLAY_ON .macro
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    CLR r30, r30, OE_PIN
    .endm

CHECK_FOR_DISPLAY_OFF .macro
    .newblock
    QBEQ END?, curBright, 0
    GET_PRU_CLOCK tmpReg1, tmpReg2, 4
    QBGT END?, tmpReg1, curBright
    DISPLAY_OFF
    LDI curBright, 0
END?:
    .endm

WAIT_FOR_DISLAY_OFF .macro
    .newblock
    QBEQ DISPLAY_ALREADY_OFF?, curBright, 0
WAIT_FOR_TIMER?:
        GET_PRU_CLOCK tmpReg1, tmpReg2, 4
        QBGT WAIT_FOR_TIMER?, tmpReg1, curBright
    DISPLAY_OFF
DISPLAY_ALREADY_OFF?:
    .endm
#else 

DISPLAY_OFF .macro
    .endm

DISPLAY_ON .macro
    .newblock
    // need to wait a bit between latch and ON
#ifdef OUTPUTS16
    // the 16 output hardware regenerates the control signals per group of
    // four outputs so the latch settles much faster than the original
    // single-buffer 8 output hardware
    LOOP NOPLOOP?, 8
#else
    LOOP NOPLOOP?, 16
#endif
        NOP
NOPLOOP?:
    XOUT  12, &curBright, 4
    .endm

CHECK_FOR_DISPLAY_OFF .macro
    .endm

WAIT_FOR_DISLAY_OFF .macro
    .newblock
BRIGHTLOADLOOP?:
    XIN 12, &curBright, 4
    QBNE BRIGHTLOADLOOP?, curBright, 0
    .endm
#endif

TOGGLE_OSHIFT .macro
    SET r30, r30, OSHIFT_PIN
    CLR r30, r30, OSHIFT_PIN
    .endm

TOGGLE_OLATCH .macro
    SET r30, r30, OLATCH_PIN
    CLR r30, r30, OLATCH_PIN
    .endm

TOGGLE_CLOCK .macro
    NOP
    CLR r30, r30, CLOCK_PIN
    NOP
    NOP
    NOP
    NOP
    NOP
    SET r30, r30, CLOCK_PIN
    .endm

TOGGLE_LATCH .macro
    NOP
    NOP
    SET r30, r30, LATCH_PIN
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    CLR r30, r30, LATCH_PIN
    NOP
    .endm

// output a full rgb/rgb2 for a pixel
//
// The panel clock (CLOCK_PIN) rising edge that captures pixel N into the
// panels is emitted in the middle of shifting pixel N+1's data instead of
// via a dedicated NOP-padded TOGGLE_CLOCK after the latch.  This costs no
// extra instructions and gives both clock phases, as well as the data setup
// time through the output buffers, roughly half a pixel time of margin -
// considerably more than the old NOP delays provided.  The caller must emit
// a final rising edge after the last pixel of a stride (see
// FINISH_STRIDE_CLOCK) since that pixel's edge would otherwise never occur.
OUTPUT_PIXEL .macro
    .newblock
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#ifdef OUTPUTS16
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#endif
    // rising edge captures the previous pixel, its data has been stable
    // on the panel data lines since its OLATCH half a pixel ago
    SET r30, r30, CLOCK_PIN
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#ifdef OUTPUTS16
    // Second bank (outputs 8-15): R1,G1,B1,R2,G2,B2
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    CLR r30, r30, CLOCK_PIN
    .endm

// The last pixel of a stride has been latched onto the data lines but its
// clock rising edge has not happened yet - allow the data to settle through
// the buffers, then clock it into the panels.  The clock is left high, which
// SETADDRESS and the first pixel of the next stride expect.
FINISH_STRIDE_CLOCK .macro
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    SET r30, r30, CLOCK_PIN
    .endm

SETADDRESS .macro
    // optimize a bit:
    // when setting address, we know the display has to be off 
    // and the latch is low, clock is high
    MOV tmpReg1.b1, curAddress
    LSL tmpReg1.b1, tmpReg1.b1, SEL0_PIN - 8
    SET tmpReg1.b1, tmpReg1.b1, OE_PIN - 8
    SET tmpReg1.b1, tmpReg1.b1, CLOCK_PIN - 8
    CLR tmpReg1.b1, tmpReg1.b1, OSHIFT_PIN - 8
    MOV r30.b1, tmpReg1.b1    
    .endm

// route to the direct SEL-pin write (modes 0/2, where the stride table
// already holds the final pin pattern) or to the row-select shift register
// routine (modes 1/3/4)
SET_ROW_ADDRESS .macro
    .newblock
    QBEQ STD?, addrMode, 0
    QBEQ STD?, addrMode, 2
    JAL  r23.w0, ROW_ADDR_SHIFT
    QBA  DONE?
STD?:
    SETADDRESS
DONE?:
    .endm

// ~120ns settle for the (slow, HC164 class) row shift registers
ADDR_DELAY .macro
    .newblock
    LDI tmpReg2.b3, 8
DLY?:
    SUB tmpReg2.b3, tmpReg2.b3, 1
    QBNE DLY?, tmpReg2.b3, 0
    .endm

DEBUGTOGGLE .macro pin
    CLR r30, r30, pin
    NOP
    NOP
    SET r30, r30, pin
    NOP
    NOP
    CLR r30, r30, pin
    NOP
    NOP
    SET r30, r30, pin
    NOP
    NOP
    CLR r30, r30, pin
    .endm


// Load the next 48 bytes of output data from the shared-memory ring.
//
// Frame data is streamed into the 32KB PRU shared RAM by a real-time thread
// on the ARM (see runPumpThread in BBShiftPanel.cpp).  DDR reads from the
// PRU (XFR2VBUS or LBBO) have a 1.1-1.8us round trip on the AM62x and cannot
// sustain the data rate that 16 outputs require, which showed up as visible
// pauses in the output clock.  Shared RAM reads are 18 cycles and fully
// predictable.  The ARM streams whole frames back to back, so both sides
// stay frame-aligned as long as they agree on the frame byte count.
//
// availBytes caches how far ahead the producer is so the producer counter
// is only re-read from shared RAM when the cached window is exhausted.
#ifdef PRU0_PREFETCH
// The 16-output two-PRU firmware can delegate shared-RAM reads to the OE
// PRU.  PRU0 fetches the next four-pixel block while this PRU shifts the
// current one, then publishes [sequence, 48 bytes] through scratchpad bank
// 11.  Bank 10 is the acknowledgement; acknowledge immediately after XIN so
// PRU0 can overwrite bank 11 while the data is safely held in r2-r13.
//
// This is deliberately unavailable to SINGLEPRU and PWM builds: their other
// PRU is respectively absent or owns the GCLK timing program.
#define prefetchSeq r14
LOAD_DATA .macro
    .newblock
    QBA   FIRSTCHECK?
WAITPREFETCH?:
    // count the spins so the ring-wait statistics stay meaningful
    ADD   stallCount, stallCount, 1
FIRSTCHECK?:
    XIN   11, &r1, 52
    QBNE  WAITPREFETCH?, r1, prefetchSeq
    XOUT  10, &prefetchSeq, 4
    ADD   prefetchSeq, prefetchSeq, 1
    LDI   dataOutReg, &outputData
    .endm
#else
LOAD_DATA .macro
    .newblock
    QBLE  HAVEDATA?, availBytes, 48
RINGWAIT?:
    ADD   stallCount, stallCount, 1
    LBBO  &tmpReg1, ringCtrl, 0, 4
    SUB   availBytes, tmpReg1, consumedCnt
    CHECK_FOR_DISPLAY_OFF
    QBGT  RINGWAIT?, availBytes, 48
HAVEDATA?:
    LBBO  &outputData, ringReadPtr, 0, 48
    ADD   ringReadPtr, ringReadPtr, 48
    ADD   consumedCnt, consumedCnt, 48
    SUB   availBytes, availBytes, 48
    SBBO  &consumedCnt, ringCtrl, 4, 4
    // ringCtrl is also the first address past the ring
    QBNE  NOWRAP?, ringReadPtr, ringCtrl
    MOV   ringReadPtr, ringBase
NOWRAP?:
    LDI   dataOutReg, &outputData
    .endm
#endif


CLEARBITS .macro
    .newblock
    CLR r30, r30, OCLR_PIN
    TOGGLE_OLATCH
    SET r30, r30, OCLR_PIN
    LOOP ENDLOOP?, numPixels
        TOGGLE_CLOCK
        NOP
        NOP
        CHECK_FOR_DISPLAY_OFF
        NOP
        NOP
ENDLOOP?:

    .endm

;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
    .sect    ".text:main"
    .global    ||main||
||main||:
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

    // Enable the XIN/XOUT shifts
    LDI32   r0, 0x03
    LDI32   r1, PRU_CONFIG_REG
    SBBO    &r0, r1, 0x34, 4
    // with the shift feature enabled, r0.b0 is the shift amount applied to
    // every XIN/XOUT - it MUST stay 0 or the bank 12 OE handshake with the
    // other PRU lands in the wrong registers
    LDI     r0.b0, 0


    // Make sure the data address and command are cleared at start
	LDI 	r1, 0x0
	LDI 	r2, 0x0
	SBCO	&r1, CONST_PRUDRAM, 0, 8

    // clear the control lines to starting values
    LDI     r30.b0, 0
    CLR     r30, r30, OSHIFT_PIN
    CLR     r30, r30, OLATCH_PIN
    SET     r30, r30, OCLR_PIN
    SET     r30, r30, OE_PIN
    CLR     r30, r30, LATCH_PIN
    CLR     r30, r30, CLOCK_PIN
    LDI     curAddress, 0
    SETADDRESS 

    // Make sure variables and such are clear
    LDI     curStride, 0
    LDI     stallCount, 0
    LDI     frameCount, 0

    // Wait for the ARM side to publish the ring location/size into our data
    // RAM.  It cannot be written before the firmware starts: the firmware
    // load clears the PRU memories and writes while the PRUSS is powered
    // down are silently dropped.  The ARM writes the base first, then the
    // (nonzero) size.  The consumed counter is published afterwards so the
    // ARM pump thread starts consistent.
    LDI     tmpReg1, SMEM_RING_CONFIG_OFFSET
RINGCFGWAIT:
    LBCO    &ringBase, CONST_PRUDRAM, tmpReg1, 8    // base, size (into availBytes)
    QBEQ    RINGCFGWAIT, availBytes, 0
    MOV     ringReadPtr, ringBase
    ADD     ringCtrl, ringBase, availBytes
#ifndef PRU0_PREFETCH
    LDI     consumedCnt, 0
    LDI     availBytes, 0
    SBBO    &consumedCnt, ringCtrl, 4, 4
#endif

#ifdef PRU0_PREFETCH
    // PRU0 starts its monotonic scratchpad sequence at one.
    LDI     prefetchSeq, 1
#endif

    // the addressing config is written before the ring config, so it is
    // valid once the ring handshake completes
    LDI     tmpReg1, ADDR_CONFIG_OFFSET
    LBCO    &r29, CONST_PRUDRAM, tmpReg1, 4
  
	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    SET     r30, r30, OE_PIN

	// Load the pointer to the buffer from PRU DRAM into data_addr and the
	// start command into commandReg
	LBCO	&data_addr, CONST_PRUDRAM, 0, 8

	// Wait for a non-zero command
	QBEQ	_LOOP, numPixels, 0

	// Command of 0xFFFF is the signal to exit
    LDI     tmpReg1, 0xFFFF
	QBNE	CHECKINIT, numPixels, tmpReg1
    JMP     EXIT
CHECKINIT:
    // Command of 0xFFF0 sends the panel configuration registers (FM6126A
    // style panels need them before they will display anything)
    LDI     tmpReg1, 0xFFF0
	QBNE	DOOUTPUT, numPixels, tmpReg1
    JMP     PANEL_INIT


DOOUTPUT
    // uncomment to do only one frame
    //LDI     tmpReg1, 0
    //SBCO    &tmpReg1, CONST_PRUDRAM, 4, 4

    //LDI     numStrides, 9

    LDI     curStride, 0
    LDI     curBright, 0
STRIDE_START:
    LDI dataOutReg, &outputData
    MOV curPixel, numPixels

#ifdef SINGLEPRU
    // if it's a very short amount of time to be on, we need to do
    // it here as the LOAD_DATA may be longer
    QBLT OUTPUTPIXELS, curBright, 100
WAIT_FOR_TIMER1:
        GET_PRU_CLOCK tmpReg1, tmpReg2, 4
        QBGT WAIT_FOR_TIMER1, tmpReg1, curBright
    DISPLAY_OFF
#endif

    LSL tmpReg1, curStride, 3
    ADD tmpReg1, tmpReg1, 15
	LBCO &tmpReg1.b0, CONST_PRUDRAM, tmpReg1, 1
    QBBC OUTPUTPIXELS, tmpReg1.b0, 7
        AND curAddress, tmpReg1.b0, 0x1F
        WAIT_FOR_DISLAY_OFF
        SET_ROW_ADDRESS
        CLEARBITS
        TOGGLE_LATCH
        LDI curBright, 20
        DISPLAY_ON
        WAIT_FOR_DISLAY_OFF

#ifdef OUTPUTS16
OUTPUTPIXELS:
    // output 4 pixels (12 bytes each = 48 bytes per LOAD_DATA)
    LOAD_DATA
    CHECK_FOR_DISPLAY_OFF
    LOOP ENDLOOPPIXEL, 4
        OUTPUT_PIXEL
ENDLOOPPIXEL:

    SUB curPixel, curPixel, 4
#else
OUTPUTPIXELS:
    // output 8 pixels (6 bytes each = 48 bytes per LOAD_DATA)
    LOAD_DATA
    CHECK_FOR_DISPLAY_OFF
    LOOP ENDLOOPPIXEL, 4
        OUTPUT_PIXEL
ENDLOOPPIXEL:
    CHECK_FOR_DISPLAY_OFF
    LOOP ENDLOOPPIXEL2, 4
        OUTPUT_PIXEL
ENDLOOPPIXEL2:

    SUB curPixel, curPixel, 8
#endif
    CHECK_FOR_DISPLAY_OFF
    QBNE OUTPUTPIXELS, curPixel, 0

    FINISH_STRIDE_CLOCK
    WAIT_FOR_DISLAY_OFF

    LSL tmpReg1, curStride, 3
    ADD tmpReg1, tmpReg1, 8
	LBCO &curBright, CONST_PRUDRAM, tmpReg1, 8
    AND curAddress, curAddress, 0x1F
DOSETADDRESS:
    SET_ROW_ADDRESS
    LDI curAddress, 0
    TOGGLE_LATCH
    DISPLAY_ON

    ADD curStride, curStride, 1
    QBNE STRIDE_START, numStrides, curStride

    WAIT_FOR_DISLAY_OFF

    // export the cumulative ring-wait iteration count and frame count so the
    // ARM side can quantify how much time is lost waiting on the pump
    ADD     frameCount, frameCount, 1
    LDI     tmpReg1, SMEM_RING_STATS_OFFSET
    SBCO    &stallCount, CONST_PRUDRAM, tmpReg1, 8

	// Go back to waiting for the next frame buffer
	JMP	_LOOP

// Clock the row number into an external row-select shift register, for the
// panel types that do not have binary address inputs.  Ported from the
// rpi-rgb-led-matrix RowAddressSetter classes:
//   mode 1: A = clock, B = data, active row bit LOW, shift scan-rows bits
//   mode 3: A = clock, C = data, active row bit HIGH, shift the whole row
//           chain (the panel height), the row bit repeated every scan rows.
//           Panels that daisy chain one row driver per band of scan rows
//           need every bit of it: shifting only one band leaves each band
//           one row behind the one before it.  A single driver shared by
//           every band keeps just the last band's worth, which is the same
//           pattern, so this is also what those panels see.
//   mode 4 (SM5266P): C = enable, B = data, A = clock; shift 8 bits with
//           the (row mod 8) bit HIGH, then D/E directly select the bank
// The display is off and the panel latch low when this runs (row tail).
// curAddress holds the row number; clobbers tmpReg1.b1-b3, tmpReg2.b2/b3.
ROW_ADDR_SHIFT:
    QBEQ ROWADDR_MODE4, addrMode, 4
    // modes 1/3: the active position in each band is (rows - 1 - row)
    SUB  tmpReg1.b1, addrRows, 1
    SUB  tmpReg1.b1, tmpReg1.b1, curAddress
    // bits to clock: one band for mode 1, the whole chain for mode 3 (an
    // unset or short chain length falls back to one band)
    MOV  tmpReg1.b3, addrRows
    QBNE ROWADDR_LEN, addrMode, 3
    QBGT ROWADDR_LEN, addrChain, addrRows
    MOV  tmpReg1.b3, addrChain
    // ~1us for the column drivers to go dark first: the very first clock
    // walks the row bit onto the next row, and the tail of the old data
    // shows there as a faint copy of each pixel on the next row scanned
    LDI  tmpReg2.b3, 125
ROWADDR_PREDLY:
    SUB  tmpReg2.b3, tmpReg2.b3, 1
    QBNE ROWADDR_PREDLY, tmpReg2.b3, 0
ROWADDR_LEN:
    LDI  tmpReg2.b2, 0                       // position within the band
    LDI  tmpReg1.b2, 0                       // bits clocked
    // Only the clock and that mode's data line move, as in rpi: mode 1
    // leaves C low and mode 3 leaves B low.  A panel is free to use the
    // other line for something else.
    CLR  r30, r30, SEL1_PIN
    CLR  r30, r30, SEL2_PIN
ROWADDR_SHIFTLOOP:
    CLR  r30, r30, SEL0_PIN
    QBEQ ROWADDR_ACTIVE, tmpReg2.b2, tmpReg1.b1
    // inactive bit: mode 1 = B HIGH, mode 3 = C LOW
    QBEQ ROWADDR_INACT3, addrMode, 3
    SET  r30, r30, SEL1_PIN
    QBA  ROWADDR_CLK
ROWADDR_INACT3:
    CLR  r30, r30, SEL2_PIN
    QBA  ROWADDR_CLK
ROWADDR_ACTIVE:
    // active bit: mode 1 = B LOW, mode 3 = C HIGH
    QBEQ ROWADDR_ACT3, addrMode, 3
    CLR  r30, r30, SEL1_PIN
    QBA  ROWADDR_CLK
ROWADDR_ACT3:
    SET  r30, r30, SEL2_PIN
ROWADDR_CLK:
    ADDR_DELAY
    SET  r30, r30, SEL0_PIN
    ADDR_DELAY
    ADD  tmpReg2.b2, tmpReg2.b2, 1
    QBNE ROWADDR_NOWRAP, tmpReg2.b2, addrRows
    LDI  tmpReg2.b2, 0
ROWADDR_NOWRAP:
    ADD  tmpReg1.b2, tmpReg1.b2, 1
    QBNE ROWADDR_SHIFTLOOP, tmpReg1.b2, tmpReg1.b3
    // mode 1 gives one more rising edge; mode 3 only returns the clock low.
    // rpi's mode 3 tail is SetBits(clock) then ClearBits(clock), and the
    // clock is already high there, so it clocks exactly the bits it sent - an
    // extra edge copies the last bit into two rows and drops the first row.
    QBEQ ROWADDR_M3TAIL, addrMode, 3
    CLR  r30, r30, SEL0_PIN
    ADDR_DELAY
    SET  r30, r30, SEL0_PIN
    JMP  r23.w0
ROWADDR_M3TAIL:
    CLR  r30, r30, SEL0_PIN
    JMP  r23.w0

ROWADDR_MODE4:
    SET  r30, r30, SEL2_PIN                  // enable serial input
    AND  tmpReg1.b1, curAddress, 0x7
    LDI  tmpReg2.b2, 8
ROWADDR_M4LOOP:
    SUB  tmpReg2.b2, tmpReg2.b2, 1
    QBNE ROWADDR_M4LOW, tmpReg2.b2, tmpReg1.b1
    SET  r30, r30, SEL1_PIN
    QBA  ROWADDR_M4CLK
ROWADDR_M4LOW:
    CLR  r30, r30, SEL1_PIN
ROWADDR_M4CLK:
    ADDR_DELAY
    SET  r30, r30, SEL0_PIN
    ADDR_DELAY
    ADDR_DELAY
    CLR  r30, r30, SEL0_PIN
    ADDR_DELAY
    QBNE ROWADDR_M4LOOP, tmpReg2.b2, 0
    CLR  r30, r30, SEL2_PIN                  // disable serial input
    // D/E select which shifter bank drives
    CLR  r30, r30, SEL3_PIN
    QBBC ROWADDR_M4NOD, curAddress, 3
    SET  r30, r30, SEL3_PIN
ROWADDR_M4NOD:
    CLR  r30, r30, SEL4_PIN
    QBBC ROWADDR_M4NOE, curAddress, 4
    SET  r30, r30, SEL4_PIN
ROWADDR_M4NOE:
    JMP  r23.w0

// Clock the panel configuration registers out to FM6126A style panels.
// The parameter block is written by the ARM at PANEL_INIT_OFFSET in data
// RAM (see sendPanelInitPackets in BBShiftPanel.cpp):
//   uint16 numRegs, uint16 columns, then per register:
//   uint16 leCount, uint16 pattern (bit i of the pattern is the data value
//   for column i mod 16, driven identically on every output's data lines)
// A register write is signaled to the panel by holding LATCH through the
// final (leCount-1) column clocks.  The display is off (OE high) the whole
// time and the ring/pump state is untouched - only constant data is
// shifted, so this can run between frames at any time.
PANEL_INIT:
    LDI     tmpReg1, PANEL_INIT_OFFSET
    LBCO    &r23, CONST_PRUDRAM, tmpReg1, 4      // r23.w0 = numRegs, r23.w2 = columns
    ADD     tmpReg1, tmpReg1, 4
    SET     r30, r30, OE_PIN
    CLR     r30, r30, LATCH_PIN
    CLR     r30, r30, CLOCK_PIN
INITREGLOOP:
    QBEQ    INITDONE, r23.w0, 0
    LBCO    &r25, CONST_PRUDRAM, tmpReg1, 4      // r25.w0 = leCount, r25.w2 = pattern
    ADD     tmpReg1, tmpReg1, 4
    MOV     curPixel, r23.w2                     // column countdown
    MOV     r27.w0, r25.w2                       // rotating pattern
INITCOLLOOP:
    // latch asserted through the last (leCount-1) columns
    QBLE    INITNOLATCH, curPixel, r25.w0
    SET     r30, r30, LATCH_PIN
INITNOLATCH:
    // all outputs' data lines get the pattern's low bit for this column
    LDI     DATA_BYTE, 0x00
    QBBC    INITSHIFT, r27, 0
    LDI     DATA_BYTE, 0xFF
INITSHIFT:
    // rotate the 16 bit pattern right one
    MOV     tmpReg2.w0, r27.w0
    LSR     r27.w0, r27.w0, 1
    QBBC    INITNOROT, tmpReg2, 0
    SET     r27, r27, 15
INITNOROT:
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
#ifdef OUTPUTS16
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    // generous data settling time through the output buffers, then clock
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    SET     r30, r30, CLOCK_PIN
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    CLR     r30, r30, CLOCK_PIN
    SUB     curPixel, curPixel, 1
    QBNE    INITCOLLOOP, curPixel, 0
    CLR     r30, r30, LATCH_PIN
    SUB     r23.w0, r23.w0, 1
    QBA     INITREGLOOP
INITDONE:
    LDI     DATA_BYTE, 0
    // clear the command so the ARM knows the registers were sent
    LDI     r23, 0
    SBCO    &r23, CONST_PRUDRAM, 4, 4
    JMP     _LOOP

EXIT:
#ifndef SINGLEPRU
    LDI32 curBright, 0xFFFFFFF
    XOUT  12, &curBright, 4
#endif

	// Write a 0 into the fields so that they know we're done
	LDI32 r2, 0
	LDI32 r3, 0
	SBCO &r2, CONST_PRUDRAM, 0, 8

	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT
