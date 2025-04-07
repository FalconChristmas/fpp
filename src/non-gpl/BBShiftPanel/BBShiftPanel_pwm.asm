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

#define OE_PIN      0

#define curBright   r25
#define extraWait   r26

#define tmpReg1     r28
#define tmpReg2     r29

DISPLAY_OFF .macro
    SET r30, r30, OE_PIN
    .endm

DISPLAY_ON .macro
    CLR r30, r30, OE_PIN
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

    // Make sure the brightness is clear at start
	LDI 	curBright, 0x0
	LDI		extraWait, 0x0
	XOUT 	12, &curBright, 8
  
	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    // make sure the display is off
    DISPLAY_OFF

	XIN 	12, &curBright, 8
	// Wait for a non-zero brightness
	QBEQ	_LOOP, curBright, 0

	// Command of 0xFFFFFFFF is the signal to exit
    LDI32     tmpReg1, 0xFFFFFFFF
	QBNE	DOOUTPUT, curBright, tmpReg1
    JMP     EXIT

DOOUTPUT:
    // turn the display on
    DISPLAY_ON

ONLOOP:
    SUB     curBright, curBright, 2
    QBLT    ONLOOP, curBright, 1

    DISPLAY_OFF

OFFLOOP:
	QBGT	NOTIFY, extraWait.w0, 3
	SUB		extraWait.w0, extraWait.w0, 3
	JMP		OFFLOOP

NOTIFY:
    LDI curBright, 0
	XOUT 12, &curBright, 4

    JMP _LOOP

EXIT:
	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT
