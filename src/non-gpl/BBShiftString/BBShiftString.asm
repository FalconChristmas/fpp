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



#define T0_TIME   275
#define T1_TIME   675
#define LOW_TIME  1100
//if LOW_TIME needs to be more than 1250, you need to do:
// #define SLOW_WAITNS

#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU1
#endif

#ifdef RUNNING_ON_PRU1
#define CLOCK_MASK 0x04
#define LATCH_MASK 0x08
#else
#define CLOCK_MASK 0x80
#define LATCH_MASK 0x40
#endif

#define FALCONV5_PERIOD  1125


#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"

/** Register map */
#define data_addr	r0
#define curCommand  r2.w0
#define next_check  r2.w2
#define cur_data	r3.w0
#define data_len    r3.w2


//r4-r7 contains the output masks
//currently just r4 and r5 (64 bits) but r6/7 
//will be needed if we jump to more than 8pins
//per shift out
#define BYTES_FOR_MASKS 8
// if we move to more than 8 pins, overflow will go to r8
#define MASK_OVERFLOW r6
#define OUTPUT_MASKS r4
//16 registers for channel data
// r10 - r25
//If we move to 16bits per shift, we'll need to 
//use XOUT to scratch pads for the additional 
//data or load 4 bits at a time instaed of 8
#define pixelData    r10

#define data_flags   r29

#define DATABLOCKSIZE 64

TOGGLE_CLOCK .macro
    LDI r30.b1, CLOCK_MASK
    NOP
    NOP
    LDI r30.b1, 0
    NOP
    .endm

TOGGLE_LATCH .macro
    LDI r30.b1, LATCH_MASK
    NOP
    NOP
    LDI r30.b1, 0
    NOP
    NOP
    .endm


OUTPUT_REG .macro REG
    .newblock
    MOV r30.b0, REG.b0
    TOGGLE_CLOCK
    MOV r30.b0, REG.b1
    TOGGLE_CLOCK
    MOV r30.b0, REG.b2
    TOGGLE_CLOCK
    MOV r30.b0, REG.b3
    TOGGLE_CLOCK
    .endm


OUTPUT_HIGH .macro
    .newblock
    OUTPUT_REG r4
    OUTPUT_REG r5
    .endm

OUTPUT_LOW .macro
    .newblock
    LDI r30.b0, 0
    NOP
    LOOP DONELOW?, 8
        TOGGLE_CLOCK
        NOP
DONELOW?:
    .endm

OUTPUT_FULL_BIT .macro REG1, REG2
    .newblock
    OUTPUT_HIGH
    //wait for the full cycle to complete
    WAITNS  LOW_TIME, r8, r9
    //start the clock
    RESET_PRU_CLOCK r8, r9
    TOGGLE_LATCH
    OUTPUT_REG REG1
    OUTPUT_REG REG2
    WAITNS  T0_TIME, r8, r9
    TOGGLE_LATCH
    OUTPUT_LOW
    WAITNS  T1_TIME, r8, r9
    TOGGLE_LATCH
    .endm

OUTPUT_FULL_BIT_FV5 .macro REG1, REG2
    OUTPUT_REG REG1
    OUTPUT_REG REG2
    WAITNS  FALCONV5_PERIOD, r8, r9
    RESET_PRU_CLOCK r8, r9
    TOGGLE_LATCH
    .endm

OUTPUT_FALCONV5_PACKET .macro
    .newblock
    QBBS  START_FALCONV5?, data_flags, 0
        JMP DONE_FALCONV5?
START_FALCONV5?:
    OUTPUT_LOW
    TOGGLE_LATCH
    RESET_PRU_CLOCK r8, r9
    LOOP HI_START?, 14
        WAITNS  FALCONV5_PERIOD, r8, r9
        RESET_PRU_CLOCK r8, r9
HI_START?:    
    OUTPUT_HIGH
    TOGGLE_LATCH
    LOOP LONG_START?, 45
        WAITNS  FALCONV5_PERIOD, r8, r9
        RESET_PRU_CLOCK r8, r9
LONG_START?:

    LDI  data_len, 56
#ifdef RUNNING_ON_PRU1
    LDI32   data_addr, 0x00011000
#else
    LDI32   data_addr, 0x00010000
#endif    
    LBBO    &r10, data_addr, 0, DATABLOCKSIZE
    ADD     data_addr, data_addr, DATABLOCKSIZE
FALCONV5_LOOP?:
    // start bit
    OUTPUT_LOW
    WAITNS  FALCONV5_PERIOD, r8, r9
    TOGGLE_LATCH
    RESET_PRU_CLOCK r8, r9
    OUTPUT_FULL_BIT_FV5 r10, r11
    OUTPUT_FULL_BIT_FV5 r12, r13
    OUTPUT_FULL_BIT_FV5 r14, r15
    OUTPUT_FULL_BIT_FV5 r16, r17
    OUTPUT_FULL_BIT_FV5 r18, r19
    OUTPUT_FULL_BIT_FV5 r20, r21
    OUTPUT_FULL_BIT_FV5 r22, r23
    OUTPUT_FULL_BIT_FV5 r24, r25
    OUTPUT_FULL_BIT_FV5 r4, r5    //stop bit
    QBEQ DONE_FALCONV5_LOOP?, data_len, 0
        LBBO    &r10, data_addr, 0, DATABLOCKSIZE
        ADD     data_addr, data_addr, DATABLOCKSIZE
        SUB     data_len, data_len, 1
        JMP FALCONV5_LOOP?
DONE_FALCONV5_LOOP?:
    WAITNS  LOW_TIME, r8, r9

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
	// Enable OCP master port
	// clear the STANDBY_INIT bit in the SYSCFG register,
	// otherwise the PRU will not be able to write outside the
	// PRU memory space and to the BeagleBon's pins.
	LBCO	&r0, C4, 4, 4
	CLR	    r0, r0, 4
	SBCO	&r0, C4, 4, 4

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

	// Write a 0x1 into the response field so that they know we have started
	LDI 	r2, 0x1
	SBCO	&r2, CONST_PRUDRAM, 8, 4

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    //make sure the clock starts
    RESET_PRU_CLOCK r8, r9

	// Load the pointer to the buffer from PRU DRAM into r0 and the
	// start command into r1
	LBCO	&data_addr, CONST_PRUDRAM, 0, 8

	// Wait for a non-zero command
	QBEQ	_LOOP, r1, 0

	// Command of 0xFFFF is the signal to exit
    LDI     r8, 0xFFFF
	QBNE	CONT_DATA, r1, r8
    JMP EXIT

CONT_DATA:
    // r1 (the command) is the data length in lower word
    MOV data_len, r1.w0
    MOV data_flags, r1.w2
    RESET_PRU_CLOCK r8, r9

    // reset command to 0 so ARM side will send more data
    LDI     r1, 0
    SBCO    &r1, CONST_PRUDRAM, 4, 4

    
    // Reset the output masks
    LBCO	&OUTPUT_MASKS, CONST_PRUDRAM, 24, BYTES_FOR_MASKS + 4
    // reset the command table
    MOV next_check, MASK_OVERFLOW
    QBBC NO_CUSTOM_CHECKS, data_flags, 1
        MOV next_check, data_len
NO_CUSTOM_CHECKS:
    LDI curCommand, 24 + BYTES_FOR_MASKS + 4
	LDI	cur_data, 1

    //start the clock
    RESET_PRU_CLOCK r8, r9

WORD_LOOP:
    LBBO    &r10, data_addr, 0, DATABLOCKSIZE
    ADD     data_addr, data_addr, DATABLOCKSIZE
    OUTPUT_FULL_BIT r10, r11
    OUTPUT_FULL_BIT r12, r13
    OUTPUT_FULL_BIT r14, r15
    OUTPUT_FULL_BIT r16, r17
    OUTPUT_FULL_BIT r18, r19
    OUTPUT_FULL_BIT r20, r21
    OUTPUT_FULL_BIT r22, r23
    OUTPUT_FULL_BIT r24, r25
    QBNE NO_COMMAND_NEEDED, cur_data, next_check
        LBCO &OUTPUT_MASKS, CONST_PRUDRAM, curCommand, BYTES_FOR_MASKS + 4
        ADD curCommand, curCommand, BYTES_FOR_MASKS + 4
        MOV next_check, MASK_OVERFLOW
NO_COMMAND_NEEDED:
    ADD cur_data, cur_data, 1
    QBGT WORD_LOOP_DONE, data_len, cur_data
    JMP WORD_LOOP

WORD_LOOP_DONE:
    OUTPUT_FALCONV5_PACKET
    NOP
    NOP
    OUTPUT_LOW
    NOP
    NOP
    TOGGLE_LATCH

    // Delay at least 300 usec; this is the required reset
	// time for the LED strip to update with the new pixels.
	SLEEPNS	300000, r8, 0

	// Go back to waiting for the next frame buffer
	JMP	_LOOP


EXIT:
	// Write a 0xFFFF into the response field so that they know we're done
	LDI r2, 0xFFFF
	SBCO &r2, CONST_PRUDRAM, 8, 4

	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT
