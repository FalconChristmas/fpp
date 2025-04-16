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
#define xshiftReg   r0.b0
#define dataOutReg  r1.b0
#define outputData  r2
#define xferR1      r18
#define xferR2      r19
#define xferR3      r20


// these can co-exist with xferR2/R3
#define tmpReg1     r19
#define tmpReg2     r20

#define data_addr   r21
#define dlen        r22.w0
#define panelWidth  r22.b2
#define numRows     r22.b3

#define curAddress  r23
#define flags		r0.b3

#define DATA_BYTE   r30.b0


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
    CLR r30, r30, DCLK_PIN
    NOP
    NOP
    SET r30, r30, DCLK_PIN
    NOP
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
	TOGGLE_OSHIFT
	TOGGLE_OLATCH
	SET r30, r30, OCLR_PIN
	.endm


LE_FOR_CLOCKS .macro clocks
	.newblock
	CLEAR_DATA_PINS
	// Turn on the LE pin
	SET r30, r30, LE_PIN
	LOOP  DONELE?, clocks
		TOGGLE_CLOCK
DONELE?:
    CLR r30, r30, LE_PIN
	.endm	


VSYNC .macro
	.newblock
	SET r30, r30, DCLK_PIN
	LE_FOR_CLOCKS   3
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
    // and the latch is low, clock is high
    MOV tmpReg1.b1, curAddress
    LSL tmpReg1.b1, tmpReg1.b1, SEL0_PIN - 8
    MOV r30.b1, tmpReg1.b1    
    .endm
 

OUTPUT_BIT_TO_ALL .macro data, bit
	.newblock
	QBBS SETBIT?, data, bit
	LDI 	dataOutReg, 0
	JMP		DATAREADY?
SETBIT?:
	LDI		dataOutReg, 0xFF	
DATAREADY?
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
    TOGGLE_OSHIFT
	TOGGLE_OLATCH
    TOGGLE_CLOCK
	.endm

INIT_FM63 .macro
	.newblock
	LE_FOR_CLOCKS 14
	LE_FOR_CLOCKS 12
	VSYNC
	LBCO	&r2, CONST_PRUDRAM, 8, 20
	SLEEPNS	1000, tmpReg1, 0
	LE_FOR_CLOCKS 14
	LOOP DONEREG1?, 16
		OUTPUT_BIT_TO_ALL r2.w0, 15
		LSL	r2.w0, r2.w0, 1
DONEREG1?:
	TOGGLE_LE
	CLEAR_DATA_PINS

	SLEEPNS	1000, tmpReg1, 0
	LE_FOR_CLOCKS 14
	LOOP DONEREG2?, 16
		OUTPUT_BIT_TO_ALL r2.w2, 15
		LSL	r2.w2, r2.w2, 1
DONEREG2?:
	TOGGLE_LE
	CLEAR_DATA_PINS


	SLEEPNS	1000, tmpReg1, 0
	LE_FOR_CLOCKS 14
	LOOP DONEREG3?, 16
		OUTPUT_BIT_TO_ALL r3.w0, 15
		LSL	r3.w0, r3.w0, 1
DONEREG3?:
	TOGGLE_LE
	CLEAR_DATA_PINS

	SLEEPNS	1000, tmpReg1, 0
	LE_FOR_CLOCKS 14
	LOOP DONEREG4?, 16
		OUTPUT_BIT_TO_ALL r3.w2, 15
		LSL	r3.w2, r3.w2, 1
DONEREG4?:
	TOGGLE_LE
	CLEAR_DATA_PINS

	SLEEPNS	1000, tmpReg1, 0
	LE_FOR_CLOCKS 14
	LOOP DONEREG5?, 16
		OUTPUT_BIT_TO_ALL r3.w2, 15
		LSL	r3.w2, r3.w2, 1
DONEREG5?:
	TOGGLE_LE
	CLEAR_DATA_PINS	

	.endm


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

UNPRELOAD_DATA .macro
    WAITFORDATA_READY
    LDI32   xferR1, 6
    MOV     xferR2, data_addr
    LDI32   xferR3, 0
    XOUT    0x60, &xferR1, 12
    XIN     0x60, &r2, 64
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
    LDI     dataOutReg, &outputData
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
    LDI     curAddress, 0
    SETADDRESS 

    // Make sure variables and such are clear
    LDI     flags, 0
    LDI     xshiftReg, 0
  
	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
	// Load the pointer to the buffer from PRU DRAM into data_addr and the
	// start command into commandReg
	LBCO	&data_addr, CONST_PRUDRAM, 0, 8

	// Wait for a non-zero command
	QBEQ	_LOOP, dlen, 0

	// Command of 0xFFFF is the signal to exit
    LDI     tmpReg1, 0xFFFF
	QBNE	DOOUTPUT, dlen, tmpReg1
    JMP     EXIT

DOOUTPUT:
	INIT_FM63
	JMP _LOOP

EXIT:
	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT
