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
 
 
 /* 
  * This works in conjuction with the BBShiftPanel_pwm.asm file to handle the GCLK line.
  * 
  * The GCLK line has to be a very steady 10mhz signal, but must be disabled during 
  * latch and row transitions
  */
  
#include "ResourceTable.asm"


#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU0
#endif


#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"

#define GCLK_PIN      0

#define SEL0_PIN	2
#define SEL1_PIN	3
#define SEL2_PIN	4
#define SEL3_PIN	5
#define SEL4_PIN	6

#define brightness  r18.b0
#define curBright	r18.b1
#define enable   	r19
#define	isRunning	r20
#define CLICKSTODO	r21


#define   CLK_HI	 (1 << GCLK_PIN)
#define   CLK_LO	 0
#define   SEL1_ONLY  (1 << SEL1_PIN)
#define   SEL1_2     ((1 << SEL2_PIN) | (1 << SEL1_PIN))
#define   SEL0_1_2   ((1 << SEL2_PIN) | (1 << SEL1_PIN) | (1 << SEL0_PIN))
#define   SEL0_1     ((1 << SEL1_PIN) | (1 << SEL0_PIN))

ONE_PULSE .macro
	.newblock
	LDI 	r30, CLK_HI
	NOP
	NOP
	//NOP
	//NOP
	//NOP
	//NOP
	//NOP
	//NOP
	//NOP
	//NOP
	NOP
	NOP
	LDI 	r30, CLK_LO
	NOP
	NOP
	NOP
	//NOP
	//NOP
	//NOP
	NOP
	.endm


DOBRIGHTLOOP .macro
	.newblock
	MOV   curBright, brightness
STARTBRIGHT?:
	QBEQ	DONEBRIGHT?, curBright, 0
		SUB curBright, curBright, 1
		SLEEPNS	900, r10, 0
		JMP STARTBRIGHT?
DONEBRIGHT?:
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

    // Make sure the gclk is off at start
	LDI		enable, 0x0
	XOUT 	12, &enable, 4
  
	CLR		r30, r30, GCLK_PIN

	// The main pwm program will set this to 1 to enable the GCLK signal
_RESETLOOP
	LDI		isRunning, 0
	XOUT	12, &isRunning, 4
	LDI		CLICKSTODO, 78
_LOOP:
	XIN 	12, &enable, 4
	// Wait for a non-zero value
	QBEQ	_RESETLOOP, enable.b0, 0

	// Command of 0xFF is the signal to exit
	QBEQ	EXIT, enable.b0, 0xFF

	LDI		isRunning, 1
	XOUT	12, &isRunning, 4
	LBCO	&brightness, CONST_PRUDRAM, 0, 1

	QBEQ	FOUR_PULSES, enable.b0, 1

	LDI		r29, 0
	LOOP  DONELOOPS, enable.b0
		DOBRIGHTLOOP
		LDI		r30, SEL1_ONLY
		DOBRIGHTLOOP
		QBNE 	NOSEL2, r29, 0
		LDI		r30, SEL1_2
		SLEEPNS	135, r10, 0
		LDI		r30, SEL0_1_2		
		SLEEPNS	135, r10, 0
NOSEL2:
		LDI		r30, SEL0_1
		DOBRIGHTLOOP
		LDI		r30, 0
		DOBRIGHTLOOP		
		MOV     r2, CLICKSTODO
		LDI		CLICKSTODO, 74
DOPULSES:			
		ONE_PULSE
		SUB		r2, r2, 1
		QBNE	DOPULSES, r2, 0
		LDI		r29, 1
DONELOOPS:
		XIN 	12, &enable, 4
		QBEQ	_RESETLOOP, enable.b0, 0
    JMP _LOOP

EXIT:
	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT


FOUR_PULSES:
	LDI enable.b0, 0
	XOUT	12,  &enable, 4
	ONE_PULSE
	NOP
	NOP
	NOP
	ONE_PULSE
	NOP
	NOP
	NOP
	ONE_PULSE
	NOP
	NOP
	NOP
	ONE_PULSE
	JMP _RESETLOOP