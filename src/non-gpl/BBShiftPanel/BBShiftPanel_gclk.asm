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
// per-chip configuration, read from PRU DRAM bytes 0-3 each pass (written by
// setupPWMRegisters in BBShiftPanel.cpp):
//   .b0 = blanking loops between GCLK packets (the brightness knob)
//   .b1 = bit 0: 0 = DP32020A style row shift register,
//                1 = direct binary row number on SEL0-4 (FM6353 style panels)
//         bit 1: FM6373 family - single short OE pulse per row instead of a
//                GCLK packet (the PWM engine runs off the constant DCLK from
//                the other PRU); implies direct row select
//         bit 2: DP3364S - per-row ROW strobe, W12 on the first row of the
//                group and W4 on the rest; honors bit 0 for the row transport
//   .b2 = GCLK pulses in the first packet after a restart (0 -> 78, FM6363)
//         FM6373 mode: the "opener" pulse width in microseconds
//         DP3364S mode: the W4 ROW pulse width in 100ns units
//   .b3 = GCLK pulses per packet after the first (0 -> 74, FM6363)
//         FM6373 mode: the row period in microseconds
//         DP3364S mode: the row period in microseconds
#define rowConfig	r22
#define curRowNum	r23
// DP3364S scan only
#define numGroups	r24.b0
#define curGroup	r25.b0


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
	// 0 marks the next GCLK packet as the first after a restart, which gets
	// its own (longer) pulse count from rowConfig.b2
	LDI		CLICKSTODO, 0
_LOOP:
	XIN 	12, &enable, 4
	// Wait for a non-zero value
	QBEQ	_RESETLOOP, enable.b0, 0

	// Command of 0xFF is the signal to exit
	QBEQ	EXIT, enable.b0, 0xFF

	LDI		isRunning, 1
	XOUT	12, &isRunning, 4
	LBCO	&rowConfig, CONST_PRUDRAM, 0, 4
	MOV		brightness, rowConfig.b0
	// unconfigured pulse counts fall back to the FM6363 values
	QBNE	HAVEPULSECFG, rowConfig.b2, 0
	LDI		rowConfig.b2, 78
	LDI		rowConfig.b3, 74
HAVEPULSECFG:

	QBEQ	FOUR_PULSES, enable.b0, 1
	QBBS	FM6373_SCAN, rowConfig.b1, 1
	QBBS	DP3364_SCAN, rowConfig.b1, 2

	LDI		r29, 0
	LDI		curRowNum, 0
	LOOP  DONELOOPS, enable.b0
		DOBRIGHTLOOP
		QBNE	DIRECTROWSEL, rowConfig.b1, 0
		// DP32020A style row shift register: inject the token on the first
		// row of each pass, then clock it along one position per row
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
		JMP		ROWSELDONE
DIRECTROWSEL:
		// direct binary row number on SEL0-4; keep the same four blanking
		// loops as the shift path so brightness behaves identically
		LSL		r30, curRowNum, SEL0_PIN
		DOBRIGHTLOOP
		DOBRIGHTLOOP
		DOBRIGHTLOOP
ROWSELDONE:
		MOV     r2, CLICKSTODO
		QBNE	NOTFIRSTPKT, r2, 0
		MOV		r2, rowConfig.b2
NOTFIRSTPKT:
		MOV		CLICKSTODO, rowConfig.b3
DOPULSES:			
		ONE_PULSE
		SUB		r2, r2, 1
		QBNE	DOPULSES, r2, 0
		LDI		r29, 1
		ADD		curRowNum, curRowNum, 1
DONELOOPS:
		XIN 	12, &enable, 4
		QBEQ	_RESETLOOP, enable.b0, 0
    JMP _LOOP

// FM6373 family scan: the chip generates its PWM internally from the
// constant DCLK; this side only advances the display row - set the row
// number on SEL0-4, pulse OE once (a few DCLK periods wide so the chip is
// guaranteed to sample it), then hold for the row period.  After a restart
// a longer "opener" pulse is sent first (DMD_STM32 sends 12 clocks before
// the frame data).  The scan free-runs between frames so the panel keeps
// multiplexing at low FPP frame rates.
FM6373_SCAN:
	QBNE	FM_NOOPENER, CLICKSTODO, 0
	LDI		CLICKSTODO, 1
	SET		r30, r30, GCLK_PIN
	MOV		curBright, rowConfig.b2
FM_OPENHIGH:
	SLEEPNS	1000, r10, 3
	SUB		curBright, curBright, 1
	QBNE	FM_OPENHIGH, curBright, 0
	CLR		r30, r30, GCLK_PIN
FM_NOOPENER:
	LDI		curRowNum, 0
FM_ROWLOOP:
	LSL		r30, curRowNum, SEL0_PIN
	SLEEPNS	135, r10, 4
	SET		r30, r30, GCLK_PIN
	SLEEPNS	600, r10, 5
	CLR		r30, r30, GCLK_PIN
	MOV		curBright, rowConfig.b3
FM_ROWWAIT:
	SLEEPNS	1000, r10, 6
	SUB		curBright, curBright, 1
	QBNE	FM_ROWWAIT, curBright, 0
	ADD		curRowNum, curRowNum, 1
	QBNE	FM_NOWRAP, curRowNum, enable.b0
	LDI		curRowNum, 0
FM_NOWRAP:
	XIN 	12, &enable, 4
	QBEQ	_RESETLOOP, enable.b0, 0
	QBEQ	EXIT, enable.b0, 0xFF
	JMP		FM_ROWLOOP

// DP3364S scan (datasheet section 10.6): the chip makes its own GCLK from an
// internal PLL, so this side only advances the row and strobes ROW once per
// row.  The rising edge of ROW starts a row's display; the first row of the
// group gets a W12 pulse and every other row a W4, which is the chip's marker
// for "top of group".  The widths are nominally counted in DCLK periods, but
// the DCLK is generated by the other PRU with no phase relationship to this
// one, so they are emitted as times derived from the configured W4 - the chip
// only has to tell the two lengths apart.
//
// Unlike the FM6373 the row transport is configurable, because a panel with
// more than 32 scan rows cannot be addressed by the 5 SEL lines at all and
// has to use the token shift register.  The scan free-runs between frames so
// the panel keeps multiplexing at low FPP frame rates.
//
// One full PWM cycle is (group count x scan rows) ROW pulses with exactly one
// W12 at the very start of it: the chip redisplays the same latched frame
// once per group to build the PWM up to its full gray depth, so the group
// counter is what W12 marks, not the row pass.  The group count is config
// register 0x03 + 1 and is passed in DRAM byte 4.
DP3364_SCAN:
	LBCO	&numGroups, CONST_PRUDRAM, 4, 1
	QBNE	DP_HAVEGROUPS, numGroups, 0
	LDI		numGroups, 64
DP_HAVEGROUPS:
	LDI		curRowNum, 0
	LDI		curGroup, 0
DP_ROWLOOP:
	QBBC	DP_SHIFTROW, rowConfig.b1, 0
	// direct binary row number on SEL0-4
	LSL		r30, curRowNum, SEL0_PIN
	JMP		DP_ROWSET
DP_SHIFTROW:
	// token shift row driver: inject the token on the first row of the pass,
	// then clock it along one position per row
	LDI		r30, SEL1_ONLY
	SLEEPNS	135, r10, 3
	QBNE	DP_NOTOKEN, curRowNum, 0
	LDI		r30, SEL1_2
	SLEEPNS	135, r10, 4
	LDI		r30, SEL0_1_2
	SLEEPNS	135, r10, 5
	JMP		DP_TOKENDONE
DP_NOTOKEN:
	LDI		r30, SEL0_1
	SLEEPNS	135, r10, 6
DP_TOKENDONE:
	LDI		r30, 0
DP_ROWSET:
	SLEEPNS	135, r10, 7
	// W4 normally, W12 (3x the configured W4 time) on row 0 of group 0
	MOV		curBright, rowConfig.b2
	QBNE	DP_HAVEWIDTH, curRowNum, 0
	QBNE	DP_HAVEWIDTH, curGroup, 0
	LSL		curBright, rowConfig.b2, 1
	ADD		curBright, curBright, rowConfig.b2
DP_HAVEWIDTH:
	SET		r30, r30, GCLK_PIN
DP_ROWHIGH:
	// the LDI inside SLEEPNS plus this SUB/QBNE are the 3 cycles that make
	// each pass of this loop a round 100ns, so the correction here is 0
	SLEEPNS	100, r10, 0
	SUB		curBright, curBright, 1
	QBNE	DP_ROWHIGH, curBright, 0
	CLR		r30, r30, GCLK_PIN
	MOV		curBright, rowConfig.b3
DP_ROWWAIT:
	SLEEPNS	1000, r10, 6
	SUB		curBright, curBright, 1
	QBNE	DP_ROWWAIT, curBright, 0
	ADD		curRowNum, curRowNum, 1
	QBNE	DP_NOWRAP, curRowNum, enable.b0
	LDI		curRowNum, 0
	ADD		curGroup, curGroup, 1
	QBNE	DP_NOWRAP, curGroup, numGroups
	LDI		curGroup, 0
DP_NOWRAP:
	XIN 	12, &enable, 4
	QBEQ	_RESETLOOP, enable.b0, 0
	QBEQ	EXIT, enable.b0, 0xFF
	JMP		DP_ROWLOOP

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