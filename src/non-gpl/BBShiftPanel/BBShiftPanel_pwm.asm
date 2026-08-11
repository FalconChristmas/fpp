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
#define RUNNING_ON_PRU0
#endif

#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"
#include "SMEMRing.hp"


// Register mappings
#define GCLK_PIN   	9
#define LE_PIN 		17
#define DCLK_PIN 	10

#define OSHIFT_PIN  8
#define OLATCH_PIN  16
#define OCLR_PIN    18

#define SEL0_PIN    11
#define SEL1_PIN    12
#define SEL2_PIN    13
#define SEL3_PIN    14
#define SEL4_PIN    15

/** Register map */
#define curReg      r0.b1
#define numReg      r0.b2
#define dataOutReg  r1.b0
#define curBlock    r1.b1
#define outputData  r2

// SMEM ring buffer (filled by the ARM pump thread, see BBShiftPanel.cpp and
// SMEMRing.hp).  Same counter convention as BBShiftPanel.asm; ringCtrl
// doubles as the ring end address for the wrap compare.
#define ringReadPtr r14
#define ringBase    r15
#define availBytes  r16
#define consumedCnt r17
#define ringCtrl    r18

#define tmpReg1     r19
#define tmpReg2     r20

#define data_addr   r21
#define command     r22.w0
#define numBlocks   r22.b2
#define numRows     r22.b3

#define curRow      r23.b0

#define DATA_BYTE   r30.b0


TOGGLE_OSHIFT .macro
    SET r30, r30, OSHIFT_PIN
    CLR r30, r30, OSHIFT_PIN
    .endm

TOGGLE_OLATCH .macro
    SET r30, r30, OLATCH_PIN
    CLR r30, r30, OLATCH_PIN
    .endm    

SET_CLOCK .macro
    NOP
    SET r30, r30, DCLK_PIN
    .endm

CLEAR_CLOCK .macro
    NOP
    CLR r30, r30, DCLK_PIN
    .endm

CLEAR_CLOCK_LE .macro
    CLR r30, r30, LE_PIN
    CLR r30, r30, DCLK_PIN
    .endm


TOGGLE_LE .macro
    NOP
    NOP
    SET r30, r30, LE_PIN
    NOP
    NOP
    NOP
    CLR r30, r30, LE_PIN
    NOP
    NOP
    .endm

CLEAR_DATA_PINS .macro
	// clear the RGB data lines
	CLR r30, r30, OCLR_PIN
	//TOGGLE_OSHIFT
	TOGGLE_OLATCH
	SET r30, r30, OCLR_PIN
	.endm


// output a full rgb/rgb2 for a pixel
OUTPUT_PIXEL .macro
    .newblock
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    CLEAR_CLOCK
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
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    SET_CLOCK
    .endm

// output a full rgb/rgb2 for a pixel
OUTPUT_PIXEL_LE .macro
    .newblock
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    CLEAR_CLOCK
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
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    SET r30, r30, LE_PIN
    SET_CLOCK
    .endm

// output a full rgb/rgb2 for a pixel
OUTPUT_PIXEL_CLRLE .macro
    .newblock
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    CLEAR_CLOCK_LE
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
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    SET_CLOCK
    .endm

OUTPUT_EMPTY_PIXEL .macro
    .newblock
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    CLEAR_CLOCK
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
#ifdef OUTPUTS16
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    SET_CLOCK
    .endm

OUTPUT_EMPTY_PIXEL_SETLE .macro
    .newblock
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    CLEAR_CLOCK
    SET r30, r30, LE_PIN
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
#ifdef OUTPUTS16
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
    LDI DATA_BYTE, 0
    TOGGLE_OSHIFT
#endif
    TOGGLE_OLATCH
    SET_CLOCK
    .endm


LE_FOR_CLOCKS_NO_CLR .macro clocks
	.newblock
    CLR     r30, r30, OCLR_PIN
    OUTPUT_EMPTY_PIXEL_SETLE
	// Turn on the LE pin
	LOOP  DONELE?, (clocks - 1)
        OUTPUT_EMPTY_PIXEL
DONELE?:
    SET     r30, r30, OCLR_PIN
	.endm

LE_FOR_CLOCKS .macro clocks
	.newblock
    CLR     r30, r30, OCLR_PIN
	LE_FOR_CLOCKS_NO_CLR clocks 
    NOP
    NOP
    NOP
    SET     r30, r30, OCLR_PIN
    CLR r30, r30, LE_PIN
    CLR r30, r30, DCLK_PIN
	.endm	
	
LOW_FOR_CLOCKS .macro clocks
    .newblock
    CLR     r30, r30, OCLR_PIN
	LOOP  DONELE?, clocks
        OUTPUT_PIXEL_CLRLE
DONELE?:
    SET     r30, r30, OCLR_PIN
    .endm

// data RAM offset of the PWM chip config (u32: b0 = chip family, 0 =
// FM6363C/FM6353 fixed-register style, 1 = FM6373 style, 2 = DP3364S),
// written by the ARM after the firmware starts.  Must match BBShiftPanel.cpp.
// (The shift firmware uses the same offset for its row addressing config.)
#define PWM_CHIP_CONFIG_OFFSET 0x1DF8




#ifdef OUTPUTS16
// 16-output versions use subroutine calls (JAL r29) to avoid
// inline macro expansion bloat that exceeds PRU IMEM (16KB).
// Subroutines OUTPUT_4_PIXELS_SUBR and OUTPUT_2_PIXELS_SUBR
// are defined near the end of the file.

DO_FULL_REGISTER .macro offset
    .newblock
    // pixels 0-3 (data already loaded in r2-r13)
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-11
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 12-15
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm
#else
DO_FULL_REGISTER .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPIXELFULLREG?, 8
        OUTPUT_PIXEL
DONEPIXELFULLREG?:
    .endm
#endif

#ifdef OUTPUTS16
// 16-output versions: 16 pixels total in 4 groups of 4
// LE timing: REG1=4 before end, REG2=6, REG3=8, REG4=10, REG5=2
// Uses JAL r29 to call subroutines, avoiding massive inline expansion.

DO_REG1_LAST .macro offset
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-11
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // LE high for pixels 12-15
    SET     r30, r30, LE_PIN
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm


DO_REG2_LAST .macro offset
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-9
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // LE high for pixels 10-11
    SET     r30, r30, LE_PIN
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // pixels 12-15
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm


DO_REG3_LAST .macro offset
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // LE high for pixels 8-15
    SET     r30, r30, LE_PIN
    // pixels 8-11
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 12-15
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm

DO_REG4_LAST .macro offset
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-5
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // LE high for pixels 6-7
    SET     r30, r30, LE_PIN
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // pixels 8-11
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 12-15
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm

DO_REG5_LAST .macro offset
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-11
    LDI     tmpReg1, (offset + 96)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 12-13
    LDI     tmpReg1, (offset + 144)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // LE high for pixels 14-15
    SET     r30, r30, LE_PIN
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    .endm

// FM6373 style register write, register-based block offset (the five FM6373
// words all latch the same way so they are sent from one loop): full chain
// blocks then the last block with LE high for the last 5 of the 16 clocks
DO_FULL_REGISTER_R .macro baseReg
    .newblock
    // pixels 0-3 (data already loaded in r2-r13)
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    ADD     tmpReg1, baseReg, 48
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-11
    ADD     tmpReg1, baseReg, 96
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 12-15
    ADD     tmpReg1, baseReg, 144
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm

DO_REG_LAST_5_R .macro baseReg
    .newblock
    // pixels 0-3
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 4-7
    ADD     tmpReg1, baseReg, 48
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    // pixels 8-9
    ADD     tmpReg1, baseReg, 96
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    JAL     r29, OUTPUT_2_PIXELS_SUBR
    // pixel 10, then LE goes high for pixels 11-15
    OUTPUT_PIXEL
    SET     r30, r30, LE_PIN
    OUTPUT_PIXEL
    // pixels 12-15
    ADD     tmpReg1, baseReg, 144
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    JAL     r29, OUTPUT_4_PIXELS_SUBR
    .endm

#else
// 8-output versions: 16 pixels total in 2 groups of 8

DO_REG1_LAST .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPRELATCH?, 4
        OUTPUT_PIXEL
DONEPRELATCH?:
    SET  r30, r30, LE_PIN
    LOOP DONEPOSTLATCH?, 4
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm


DO_REG2_LAST .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPRELATCH?, 2
        OUTPUT_PIXEL
DONEPRELATCH?:
    SET  r30, r30, LE_PIN
    LOOP DONEPOSTLATCH?, 6
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm


DO_REG3_LAST .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    SET  r30, r30, LE_PIN
    LOOP DONEPOSTLATCH?, 8
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm

DO_REG4_LAST .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DOPRELATCH?, 6
        OUTPUT_PIXEL
DOPRELATCH?
    SET  r30, r30, LE_PIN
    OUTPUT_PIXEL
    OUTPUT_PIXEL
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPOSTLATCH?, 8
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm

DO_REG5_LAST .macro offset
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    LDI     tmpReg1, (offset + 48)
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPRELATCH?, 6
        OUTPUT_PIXEL
DONEPRELATCH?:
    SET  r30, r30, LE_PIN
    LOOP DONEPOSTLATCH?, 2
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm

// FM6373 style register write, register-based block offset (the five FM6373
// words all latch the same way so they are sent from one loop): full chain
// blocks then the last block with LE high for the last 5 of the 16 clocks
DO_FULL_REGISTER_R .macro baseReg
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    ADD     tmpReg1, baseReg, 48
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPIXELFULLREG?, 8
        OUTPUT_PIXEL
DONEPIXELFULLREG?:
    .endm

DO_REG_LAST_5_R .macro baseReg
    .newblock
    LDI     r1.b0, &r2
    LOOP DONEHALFREG?, 8
        OUTPUT_PIXEL
DONEHALFREG?
    ADD     tmpReg1, baseReg, 48
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    LDI     r1.b0, &r2
    LOOP DONEPRELATCH?, 3
        OUTPUT_PIXEL
DONEPRELATCH?:
    SET  r30, r30, LE_PIN
    LOOP DONEPOSTLATCH?, 5
        OUTPUT_PIXEL
DONEPOSTLATCH?
    .endm
#endif






// Load the next 48 bytes of output data from the shared-memory ring.
//
// Frame data is streamed into the 32KB PRU shared RAM by a real-time thread
// on the ARM (see runPumpThread in BBShiftPanel.cpp and SMEMRing.hp); the
// XFR2VBUS/DDR reads this replaced have a 1.1-1.8us round trip on the AM62x
// and stalled the data shifting.  Only the DATA command consumes ring data;
// the register configs come from the PRU data RAM.
LOAD_DATA .macro
    .newblock
    QBLE  HAVEDATA?, availBytes, 48
RINGWAIT?:
    LBBO  &tmpReg1, ringCtrl, 0, 4
    SUB   availBytes, tmpReg1, consumedCnt
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







STOP_GCLK .macro
    .newblock
    //Disable GCLK and wait for it to stop
    LDI r19, 0
    XOUT  12, &r19, 4
WAITFORSTOPPED?:
    OUTPUT_EMPTY_PIXEL
    XIN   12, &r20, 4
    QBNE  WAITFORSTOPPED?, r20, 0
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
    // every XIN/XOUT - it MUST stay 0 or the bank 12 GCLK handshake with the
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
    CLR     r30, r30, GCLK_PIN
    CLR     r30, r30, LE_PIN
    CLR     r30, r30, DCLK_PIN

    // Wait for the ARM side to publish the ring location/size into our data
    // RAM (see SMEMRing.hp); it is written after the firmware starts since
    // the firmware load clears the PRU memories.  The base is written first,
    // then the (nonzero) size.  Publish the consumed counter so the ARM pump
    // thread starts consistent.
    LDI     tmpReg1, SMEM_RING_CONFIG_OFFSET
RINGCFGWAIT:
    LBCO    &ringBase, CONST_PRUDRAM, tmpReg1, 8    // base, size (into availBytes)
    QBEQ    RINGCFGWAIT, availBytes, 0
    MOV     ringReadPtr, ringBase
    ADD     ringCtrl, ringBase, availBytes
    LDI     consumedCnt, 0
    LDI     availBytes, 0
    SBBO    &consumedCnt, ringCtrl, 4, 4
  
	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    NOP
	NOP
	NOP
	NOP
	NOP
	SET 	r30, r30, DCLK_PIN
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	CLR 	r30, r30, DCLK_PIN
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	// Load the pointer to the buffer from PRU DRAM into data_addr and the
	// start command into commandReg
	LBCO	&data_addr, CONST_PRUDRAM, 0, 8

	// Wait for a non-zero command
	QBEQ	_LOOP, command, 0

	// Command of 0xFFFF is the signal to exit
    LDI     tmpReg1, 0xFFFF
	QBNE	NOEXIT, command, tmpReg1
        JMP EXIT
NOEXIT:
    //Clear the command so the ARM side can queue up the next command
    LDI     tmpReg1, 0
    SBCO    &tmpReg1, CONST_PRUDRAM, 4, 2

    QBBC    SKIPSYNC, command, 0
    STOP_GCLK
    LOW_FOR_CLOCKS 2
	LE_FOR_CLOCKS_NO_CLR 3
    LOW_FOR_CLOCKS 36

SKIPSYNC:
    QBBC    SKIPREGISTERS, command, 1
    STOP_GCLK
    JMP OUTPUT_REGISTERS

SKIPREGISTERS:
    QBBC    SKIPSTARTGCLK, command, 2
    LOW_FOR_CLOCKS 4
    MOV     tmpReg1, numRows
    XOUT    12, &tmpReg1, 4
    LOW_FOR_CLOCKS 4

SKIPSTARTGCLK:
    QBBC    SKIPDATA, command, 3
    JMP  OUTPUT_DATA

SKIPDATA:
    JMP  _LOOP





OUTPUT_DATA:
    MOV   curRow, numRows
DOROWOUTPUT:
    QBEQ  DONEDATAOUT, curRow, 0
    LDI   curReg, 16

DOREGISTEROUTPUT:
    QBEQ  DONEREGISTEROUT, curReg, 0
    MOV   curBlock, numBlocks
#ifdef OUTPUTS16
// each block is 16 pixels (matching numBlocks = rowLen / 16), which for 16
// outputs is four 48 byte loads of four pixels each
DOROWOUT:
    QBEQ  LASTINROW, curBlock, 1
        LOAD_DATA
        JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
        LOAD_DATA
        JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
        LOAD_DATA
        JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
        LOAD_DATA
        JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
        SUB   curBlock, curBlock, 1
        JMP   DOROWOUT
LASTINROW:
    LOAD_DATA
    JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
    LOAD_DATA
    JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
    LOAD_DATA
    JAL     r29, OUTPUT_4_PIXELS_CLRLE_SUBR
    LOAD_DATA
    LDI     r1.b0, &r2
    LDI     r28, 3
LASTINROW_CLRLE_LOOP:
    JAL     r29, OUTPUT_PIXEL_CLRLE_SUBR
    SUB     r28, r28, 1
    QBNE    LASTINROW_CLRLE_LOOP, r28, 0
    OUTPUT_PIXEL_LE
#else
DOROWOUT:
    QBEQ  LASTINROW, curBlock, 1
        LOAD_DATA
        LOOP ENDLOOPPIXELFIRST8, 8
            OUTPUT_PIXEL_CLRLE
ENDLOOPPIXELFIRST8:
        LOAD_DATA
        LOOP ENDLOOPPIXEL, 8
            OUTPUT_PIXEL_CLRLE
ENDLOOPPIXEL:
        SUB   curBlock, curBlock, 1
        JMP   DOROWOUT
LASTINROW:
    LOAD_DATA
    LOOP ENDLOOPPIXEL2, 8
        OUTPUT_PIXEL_CLRLE
ENDLOOPPIXEL2:
    LOAD_DATA
    LOOP ENDLOOPPIXEL3, 7
        OUTPUT_PIXEL_CLRLE
ENDLOOPPIXEL3:
    OUTPUT_PIXEL_LE
#endif
    SUB curReg, curReg, 1
    JMP DOREGISTEROUTPUT
DONEREGISTEROUT:    
    SUB     curRow, curRow, 1
    JMP     DOROWOUTPUT
DONEDATAOUT:
#ifdef OUTPUTS16
    LDI     r28, 7
DONEDATAOUT_LOOP:
    JAL     r29, OUTPUT_PIXEL_CLRLE_SUBR
    SUB     r28, r28, 1
    QBNE    DONEDATAOUT_LOOP, r28, 0
#else
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
#endif
    JMP SKIPDATA



OUTPUT_REGISTERS:
    .newblock
    // r26 holds the whole chip config for the duration of the upload:
    // b0 = family, b1 = register slot count, b2 = middle LAT burst length
    LDI     tmpReg1, PWM_CHIP_CONFIG_OFFSET
    LBCO    &r26, CONST_PRUDRAM, tmpReg1, 4
    QBNE    NOTFM6373?, r26.b0, 1
    JMP     OUTPUT_REGISTERS_FM6373
NOTFM6373?:
    QBNE    NOTDP3364?, r26.b0, 2
    JMP     OUTPUT_REGISTERS_DP3364
NOTDP3364?:
    // Signal the GCLK to do 4 ticks
    LOW_FOR_CLOCKS 2
    LDI   r19, 1
    XOUT  12, &r19, 4
    LOW_FOR_CLOCKS 1

	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 7
	LE_FOR_CLOCKS_NO_CLR 12
    LOW_FOR_CLOCKS 6

#ifdef OUTPUTS16
    // 16-output register offsets: each register is 16 pixels * 12 bytes = 192 bytes
    // With 16-byte header: 16, 208, 400, 592, 784
#define REG1_OFF 16
#define REG2_OFF 208
#define REG3_OFF 400
#define REG4_OFF 592
#define REG5_OFF 784
#else
    // 8-output register offsets: each register is 16 pixels * 6 bytes = 96 bytes
    // With 16-byte header: 16, 112, 208, 304, 400
#define REG1_OFF 16
#define REG2_OFF 112
#define REG3_OFF 208
#define REG4_OFF 304
#define REG5_OFF 400
#endif

    //-----------
    // Do register #1
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG1_LOOP:
    LDI     tmpReg1, REG1_OFF
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    REG1_LAST, curReg,  1
    DO_FULL_REGISTER    REG1_OFF
    SUB     curReg, curReg, 1
    JMP     REG1_LOOP
REG1_LAST:
    DO_REG1_LAST        REG1_OFF
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #2
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG2_LOOP:
    LDI     tmpReg1, REG2_OFF
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    REG2_LAST, curReg,  1
    DO_FULL_REGISTER    REG2_OFF
    SUB     curReg, curReg, 1
    JMP     REG2_LOOP
REG2_LAST:
    DO_REG2_LAST        REG2_OFF
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #3
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG3_LOOP:
    LDI     tmpReg1, REG3_OFF
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    REG3_LAST, curReg,  1
    DO_FULL_REGISTER    REG3_OFF
    SUB     curReg, curReg, 1
    JMP     REG3_LOOP
REG3_LAST:
    DO_REG3_LAST        REG3_OFF
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #4
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG4_LOOP:
    LDI     tmpReg1, REG4_OFF
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    REG4_LAST, curReg,  1
    DO_FULL_REGISTER    REG4_OFF
    SUB     curReg, curReg, 1
    JMP     REG4_LOOP
REG4_LAST:
    DO_REG4_LAST        REG4_OFF
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #5
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG5_LOOP:
    LDI     tmpReg1, REG5_OFF
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    REG5_LAST, curReg,  1
    DO_FULL_REGISTER    REG5_OFF
    SUB     curReg, curReg, 1
    JMP     REG5_LOOP
REG5_LAST:
    DO_REG5_LAST        REG5_OFF
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6

    JMP SKIPREGISTERS


// FM6373 family register upload (from DMD_STM32 load_config_regs and the
// kingdo9 rpi-rgb-led-matrix captures): vsync + an optional 11 and a 14 clock
// LAT burst, then N 16-bit words - the 0x00AA/0x01AA write-enable pair, one
// word of the chip's config register sequence (the ARM rotates it through the
// whole sequence across frames), an optional chip specific word, and the
// 0x0055/0x0155 commit pair - each latched with LE high for the last 5 clocks.
// The vsync lives here, so the ARM does not set PWM_COMMAND_SYNC for this
// chip family.
//
// FM6373 and ICND1065L send the 11 clock burst and five slots; SM16380SH
// skips the burst and sends six.  Both come from r26 (see OUTPUT_REGISTERS).
//
// The LAT-low spacers stay at a uniform 8 clocks here.  kingdo9's captures
// have them varying per chip and per burst (12/3/9 for ICND1065L, 6/8 for
// SM16380SH); these are idle gaps between latched words rather than anything
// the chips count, so the uniform value is the first thing to revisit if a
// panel takes the registers but comes up wrong.
OUTPUT_REGISTERS_FM6373:
    .newblock
	LE_FOR_CLOCKS_NO_CLR 3
    LOW_FOR_CLOCKS 8
    QBEQ    FMREGS_NOMID, r26.b2, 0
	LE_FOR_CLOCKS_NO_CLR 11
    LOW_FOR_CLOCKS 8
FMREGS_NOMID:
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 8

    // every word latches identically (LE for the last 5 clocks), so one loop
    // walks the register slots; r24 = slot offset, r25.b0 = count
    LDI     r24, REG1_OFF
    MOV     r25.b0, r26.b1
FMREGS_LOOP:
    MOV     curReg, numBlocks
FMREG_CHAIN:
    MOV     tmpReg1, r24
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    FMREG_CHAINLAST, curReg,  1
    DO_FULL_REGISTER_R  r24
    SUB     curReg, curReg, 1
    JMP     FMREG_CHAIN
FMREG_CHAINLAST:
    DO_REG_LAST_5_R     r24
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 8
#ifdef OUTPUTS16
    ADD     r24, r24, 192
#else
    ADD     r24, r24, 96
#endif
    SUB     r25.b0, r25.b0, 1
    QBNE    FMREGS_LOOP, r25.b0, 0

    JMP SKIPREGISTERS


// DP3364S register write (datasheet section 10.4/10.5): VSYNC (LE for 3
// clocks), PRE_ACT (LE for 14), then WR_CFG - one {addr, data} word shifted
// through the whole chain with LE high for the last 5 of its 16 clocks.  The
// data lines are "not care" during the two bare LE pulses.
//
// Only one register may be written per frame, so a single word slot is sent
// and the ARM rotates it through the 15 config registers across frames (see
// writeDP3364SeqWord).  The VSYNC lives here, so the ARM does not set
// PWM_COMMAND_SYNC for this chip.
OUTPUT_REGISTERS_DP3364:
    .newblock
	LE_FOR_CLOCKS_NO_CLR 3
    LOW_FOR_CLOCKS 8
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 8

    LDI     r24, REG1_OFF
    MOV     curReg, numBlocks
DP3364REG_CHAIN:
    MOV     tmpReg1, r24
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48
    QBEQ    DP3364REG_CHAINLAST, curReg,  1
    DO_FULL_REGISTER_R  r24
    SUB     curReg, curReg, 1
    JMP     DP3364REG_CHAIN
DP3364REG_CHAINLAST:
    DO_REG_LAST_5_R     r24
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 8

    JMP SKIPREGISTERS








#ifdef OUTPUTS16
;*****************************************************************************
; Subroutines for 16-output mode to reduce code size.
; Called via JAL r29, <label>; return via JMP r29.
; r28 is available as a loop counter for manual loops.
;*****************************************************************************

// Output 4 pixels from data in r2-r13, sets r1.b0 = &r2
OUTPUT_4_PIXELS_SUBR:
    LDI     r1.b0, &r2
    LOOP    OUTPUT_4_PIXELS_DONE, 4
        OUTPUT_PIXEL
OUTPUT_4_PIXELS_DONE:
    JMP     r29

// Output 4 pixels with CLRLE from data in r2-r13, sets r1.b0 = &r2
OUTPUT_4_PIXELS_CLRLE_SUBR:
    LDI     r1.b0, &r2
    LOOP    OUTPUT_4_CLRLE_DONE, 4
        OUTPUT_PIXEL_CLRLE
OUTPUT_4_CLRLE_DONE:
    JMP     r29

// Output 2 pixels from current r1.b0 position (caller must set r1.b0)
OUTPUT_2_PIXELS_SUBR:
    LOOP    OUTPUT_2_PIXELS_DONE, 2
        OUTPUT_PIXEL
OUTPUT_2_PIXELS_DONE:
    JMP     r29

// Output 1 pixel with CLRLE from current r1.b0 position
OUTPUT_PIXEL_CLRLE_SUBR:
    OUTPUT_PIXEL_CLRLE
    JMP     r29
#endif


EXIT:
    LDI     tmpReg1, 0xFF
    XOUT    12, &tmpReg1, 4

	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT



