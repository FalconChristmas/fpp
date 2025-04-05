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



#define T0_TIME   220
#define T1_TIME   750
#define LOW_TIME  1120
//if LOW_TIME needs to be more than 1250, you need to do:
// #define SLOW_WAITNS

#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU1
#endif

#ifdef AM33XX
#define CONTROL_BYTE r30.b1
#define DATA_BYTE r30.b0
#ifdef RUNNING_ON_PRU1
#define CLOCK_PIN 2
#define LATCH_PIN 3
#define ENABLE_PIN 0
#else
#define CLOCK_PIN 7
#define LATCH_PIN 6
#define ENABLE_PIN 0
#endif
#else
#define CONTROL_BYTE r30.b2
#define DATA_BYTE r30.b0
#define CLOCK_PIN 3
#define LATCH_PIN 0
#define ENABLE_PIN 2
#endif

ENABLE_SEND .macro
    SET CONTROL_BYTE, CONTROL_BYTE, ENABLE_PIN
    .endm

DISABLE_SEND .macro
    CLR CONTROL_BYTE, CONTROL_BYTE, ENABLE_PIN
    .endm

#define FALCONV5_PERIOD  1130


#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"

/** Register map */

//16 registers for channel data
// r2 - r17
#define pixelData    r2

#define xferR1          r18
#define xferR2          r19
#define xferR3          r20

#define tmpReg1         r19   // can co-exist with xferR2/xferR3
#define tmpReg2         r20

//r21-r24 contains the output masks, r21/22 are high, r23/24 are low
// read two extra bytes for the "next"
#define BYTES_FOR_MASKS 18
#define OUTPUT_MASKS r21
#define OUTPUT_HI_MASKS r21
#define OUTPUT_LOW_MASKS r23
#define MASK_OVERFLOW r25.w0

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


STARTXFRBUS .macro bus, dataAddress
    LDI32   xferR1, 7
    MOV     xferR2, dataAddress
    LDI32   xferR3, 0
    XOUT    bus, &xferR1, 12
    .endm


WAITFORDATA_READY .macro bus
WAITFORDATA1?:
    XIN     bus, &xferR1, 4
    QBBC    WAITFORDATA1?, xferR1, 2
WAITFORDATA2?:
    XIN     bus, &xferR1, 4
    QBBS    WAITFORDATA2?, xferR1, 3
    NOP     // spec says we need an extra NOP after this flag is set
    .endm    

PRELOAD_DATA .macro dataAddress
    WAITFORXFRBUS 0x60
    STARTXFRBUS 0x60, dataAddress
    .endm

LOAD_NEXT_DATABLOCK .macro dataAddress
    WAITFORDATA_READY 0x60
    XIN     0x60, &r2, 64
    ADD     dataAddress, dataAddress, 64
    .endm

UNPRELOAD_DATA .macro dataAddress
    WAITFORDATA_READY 0x60
    LDI32   xferR1, 6
    MOV     xferR2, dataAddress
    LDI32   xferR3, 0
    XOUT    0x60, &xferR1, 12
    XIN     0x60, &r2, 64
    .endm
#endif


TOGGLE_CLOCK .macro
    SET CONTROL_BYTE, CONTROL_BYTE, CLOCK_PIN
    NOP
    CLR CONTROL_BYTE, CONTROL_BYTE, CLOCK_PIN
    //NOP
    .endm

TOGGLE_LATCH .macro
    SET CONTROL_BYTE, CONTROL_BYTE, LATCH_PIN
    NOP
    CLR CONTROL_BYTE, CONTROL_BYTE, LATCH_PIN
    NOP
    .endm

OUTPUT_REG_INDIRECT .macro 
    .newblock
    LOOP ENDLOOP?, 8
    MVIB DATA_BYTE, *r1.b0++
    TOGGLE_CLOCK
ENDLOOP?:
    .endm

OUTPUT_HIGH .macro
    .newblock
    MOV r1.b1, r1.b0
    LDI r1.b0, &OUTPUT_HI_MASKS
    OUTPUT_REG_INDIRECT
    MOV r1.b0, r1.b1
    .endm

OUTPUT_LOW .macro
    .newblock
    MOV r1.b1, r1.b0
    LDI r1.b0, &OUTPUT_LOW_MASKS
    OUTPUT_REG_INDIRECT
    MOV r1.b0, r1.b1
    .endm


OUTPUT_FALCONV5_PACKET .macro
    .newblock
    QBBC  DONE_FALCONV5?, data_flags, 1

START_FALCONV5?:
    OUTPUT_LOW
    TOGGLE_LATCH
	SLEEPNS	15300, tmpReg1, 0

    OUTPUT_HIGH
    TOGGLE_LATCH
	SLEEPNS	53500, tmpReg1, 0

FALCONV5_SETUP_LOOP?:
    PRELOAD_DATA  fv5_data_addr
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
    UNPRELOAD_DATA fv5_data_addr
    SLEEPNS 70000, tmpReg1, 0
    JMP FALCONV5_SETUP_LOOP?

DO_FALCONV5_LISTNER?:
    QBBC  DONE_FALCONV5?, data_flags, 3
    OUTPUT_LOW
    TOGGLE_LATCH
    SLEEPNS	15500, tmpReg1, 0
    OUTPUT_HIGH
    TOGGLE_LATCH
    SLEEPNS	20000, tmpReg1, 0

    DISABLE_SEND
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

	// Write a 0x1 into the response field so that they know we have started
	LDI 	r2, 0x1
	SBCO	&r2, CONST_PRUDRAM, 8, 4

    // Make sure the data address and command are cleared at start
	LDI 	r1, 0x0
	LDI 	r2, 0x0
	SBCO	&r1, CONST_PRUDRAM, 0, 8

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    //make sure the clock starts
    RESET_PRU_CLOCK tmpReg1, tmpReg2

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
    LBCO	&OUTPUT_MASKS, CONST_PRUDRAM, 24, BYTES_FOR_MASKS
    // reset the command table
    MOV next_check, MASK_OVERFLOW
    QBBC NO_CUSTOM_CHECKS, data_flags, 0
        MOV next_check, data_len
NO_CUSTOM_CHECKS:
    LDI curCommand, 24 + BYTES_FOR_MASKS
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
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    JAL r1.w2, OUTPUT_FULL_BIT
    QBNE NO_COMMAND_NEEDED, cur_data, next_check
        LBCO &OUTPUT_MASKS, CONST_PRUDRAM, curCommand, BYTES_FOR_MASKS
        ADD curCommand, curCommand, BYTES_FOR_MASKS
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
    WAITNS_LOOP  LOW_TIME, tmpReg1, tmpReg2
    //start the clock
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    TOGGLE_LATCH
    OUTPUT_REG_INDIRECT
    WAITNS_LOOP  T0_TIME, tmpReg1, tmpReg2
    TOGGLE_LATCH
    OUTPUT_LOW
    WAITNS_LOOP  T1_TIME, tmpReg1, tmpReg2
    TOGGLE_LATCH
    JMP r1.w2

OUTPUT_FULL_BIT_FV5:
    OUTPUT_REG_INDIRECT
    WAITNS_LOOP  FALCONV5_PERIOD, tmpReg1, tmpReg2
    TOGGLE_LATCH
    RESET_PRU_CLOCK tmpReg1, tmpReg2
    JMP r1.w2
