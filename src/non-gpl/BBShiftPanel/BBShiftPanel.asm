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



#define OE_PIN   9
#define LATCH_PIN 17
#define CLOCK_PIN 10

#define OSHIFT_PIN  8
#define OLATCH_PIN  16
#define OCLR_PIN    18

#define SEL0_PIN    11
#define SEL1_PIN    12
#define SEL2_PIN    13
#define SEL3_PIN    14
#define SEL4_PIN    15

#define SEL_MASK (0 | (1<<SEL0_PIN) | (1<<SEL1_PIN) | (1<<SEL2_PIN) | (1<<SEL3_PIN) | (1<<SEL4_PIN))

/** Register map */
#define xshiftReg   r0.b0
#define dataOutReg  r1.b0
#define outputData  r2
#define xferR1      r18
#define xferR2      r19
#define xferR3      r20

#define flags       r23
#define curPixel    r24.w0
#define curStride   r24.w2
#define curBright   r25
#define curAddress  r25.b3
#define data_addr	r26
#define numPixels   r27.w0
#define numStrides  r27.w2

#define tmpReg1     r28
#define tmpReg2     r29

#define DATA_BYTE   r30.b0

#ifdef SINGLEPRU
DISPLAY_OFF .macro
    SET r30, r30, OE_PIN
    .endm

DISPLAY_ON .macro
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
    NOP
    SET r30, r30, OSHIFT_PIN
    NOP
    CLR r30, r30, OSHIFT_PIN
    .endm

TOGGLE_OLATCH .macro
    NOP
    SET r30, r30, OLATCH_PIN
    NOP
    CLR r30, r30, OLATCH_PIN
    .endm    

TOGGLE_CLOCK .macro
    NOP
    NOP
    CLR r30, r30, CLOCK_PIN
    NOP
    NOP
    SET r30, r30, CLOCK_PIN
    NOP
    NOP
    NOP
    .endm

TOGGLE_LATCH .macro
    NOP
    NOP
    SET r30, r30, LATCH_PIN
    NOP
    NOP
    NOP
    NOP
    CLR r30, r30, LATCH_PIN
    NOP
    NOP
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
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_OSHIFT
    TOGGLE_OLATCH

    TOGGLE_CLOCK
    .endm

SETADDRESS .macro
    // optimize a bit:
    // when setting address, we know the display has to be off 
    // and the latch/clocks are all low 
    MOV tmpReg1.b1, curAddress
    LSL tmpReg1.b1, tmpReg1.b1, SEL0_PIN - 8
    SET tmpReg1.b1, tmpReg1.b1, OE_PIN - 8
    MOV r30.b1, tmpReg1.b1    
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


#define XFERBUS
#ifdef XFERBUS

WAITFORXFRBUS .macro bus
    .newblock
WAITXFRLOOP?:
    XIN     bus, &xferR1, 4
    // check to see if it's "busy" due to data waiting to be read, if so, clear it
    QBBC    NODATA?, xferR1, 2     
        XIN bus, &r2, 32
        XIN bus, &xferR1, 4
NODATA?:
    QBBS    WAITXFRLOOP?, xferR1, 0
    .endm


STARTXFRBUS .macro bus
    LDI32   xferR1, 7
    MOV     xferR2, data_addr
    LDI32   xferR3, 0
    XOUT    bus, &xferR1, 12
    .endm


PRELOAD_DATA .macro
    WAITFORXFRBUS 0x60
    STARTXFRBUS 0x60    
    .endm


WAITFORDATA_READY .macro
WAITFORDATA1?:
    XIN     0x60, &xferR1, 4
    QBBC    WAITFORDATA1?, xferR1, 2
WAITFORDATA2?:
    XIN     0x60, &xferR1, 4
    QBBS    WAITFORDATA2?, xferR1, 3
    NOP     // spec says we need an extra NOP after this flag is set
    .endm

LOAD_DATA .macro
    .newblock

    QBEQ  READPART1?, flags, 0
    QBEQ  READPART2?, flags, 1
    QBEQ  READPART3?, flags, 2

    XIN     10, &r2, 48
    LDI     flags, 0
    JMP     ENDREAD?
READPART1?:
    WAITFORDATA_READY
    XIN     0x60, &r2, 64
    LDI     flags, 1
    JMP     ENDREAD?
READPART2?:
    LDI     xshiftReg, 18
    XOUT    10, &r14, 16
    LDI     xshiftReg, 0
    WAITFORDATA_READY
    XIN     0x60, &r2, 64
    LDI     xshiftReg, 4
    XOUT    10, &r2, 32
    LDI     xshiftReg, 22
    XOUT    11, &r10, 32
    LDI     xshiftReg, 0
    XIN     10, &r2, 48
    LDI     flags, 2
    JMP     ENDREAD?
READPART3?:
    WAITFORDATA_READY
    XIN     0x60, &r2, 64
    LDI     xshiftReg, 8
    XOUT    11, &r2, 16
    LDI     xshiftReg, 26
    XOUT    10, &r6, 48
    LDI     xshiftReg, 0
    XIN     11, &r2, 48
    LDI     flags, 3    
ENDREAD?:
    LDI     dataOutReg, 8    
    .endm

#else

LOAD_DATA .macro
    .newblock
    LBBO    &outputData, data_addr, 0, 48
    ADD     data_addr, data_addr, 48
    LDI     dataOutReg, 8
    .endm

#define PRELOAD_DATA
#endif


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
    LDI     flags, 0
    LDI     curStride, 0
    LDI     xshiftReg, 0
  
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
	QBNE	DOOUTPUT, numPixels, tmpReg1
    JMP     EXIT


DOOUTPUT
    PRELOAD_DATA

    // uncomment to do only one frame
    //LDI     tmpReg1, 0
    //SBCO    &tmpReg1, CONST_PRUDRAM, 4, 4

    LDI     flags, 0
    LDI     curStride, 0
    LDI     curBright, 0
STRIDE_START:
    LDI dataOutReg, 8
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

OUTPUTPIXELS:
    // output 8 pixels
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
    CHECK_FOR_DISPLAY_OFF
    QBNE OUTPUTPIXELS, curPixel, 0

    WAIT_FOR_DISLAY_OFF

    LSL tmpReg1, curStride, 2
    ADD tmpReg1, tmpReg1, 8
	LBCO &curBright, CONST_PRUDRAM, tmpReg1, 4
    SETADDRESS
    LDI curAddress, 0
    TOGGLE_LATCH
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    DISPLAY_ON
    ADD curStride, curStride, 1
    QBNE STRIDE_START, numStrides, curStride

    WAIT_FOR_DISLAY_OFF

	// Go back to waiting for the next frame buffer
	JMP	_LOOP

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
