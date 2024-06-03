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
 

#include "ResourceTable.asm"


#if !defined(RUNNING_ON_PRU0) && !defined(RUNNING_ON_PRU1)
#define RUNNING_ON_PRU0
#endif

// 145 allows roughly 5x super sampling
#define FALCONV5_PERIOD  145

#include "FalconUtils.asm"
#include "FalconPRUDefs.hp"

/** Register map */

#define commandReg      r2
#define lengthReg       r3
#define smemLocReg      r4
#define treg1           r6
#define treg2           r7
#define enableReg       r8
#define hasDataReg      r9
#define data_reg        r10

#define BYTES_PER_STORE 12


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

    // Make sure the command and length cleared at start
	LDI 	r1, 0x0
	LDI 	r2, 0x0
	LDI 	r3, 0x0
	SBCO	&r1, CONST_PRUDRAM, 0, 12

    LDI32   smemLocReg, 0x00010000

	

IDLE_LOOP:
	// start command into commandReg
	LBCO	&commandReg, CONST_PRUDRAM, 0, 4
    LDI     treg1, 0xFFFF
	QBEQ	EXIT, commandReg, treg1

    XIN     10, &enableReg, 8
    QBNE    IDLE_LOOP, enableReg, 1

    // we are now enabled, start trying to get data
    // reset the length
    LDI     lengthReg, 0
    SBCO    &lengthReg, CONST_PRUDRAM, 4, 4

    // grab the current values and loop until something changes
    MOV     r10.b0, r31.b0
WAIT_FOR_DATA:
    XIN     10, &enableReg, 8
    QBNE    IDLE_LOOP, enableReg, 1
    QBEQ    WAIT_FOR_DATA, r10.b0, r31.b0
    MOV     r10.b1, r31.b0;
    RESET_PRU_CLOCK treg1, treg2
    LDI     r1.b0, 42

    // we now have some data, let the other PRU know
    LDI     hasDataReg, 1
    XOUT    10, &hasDataReg, 4

BIT_LOOP:
    WAITNS_LOOP  FALCONV5_PERIOD, treg1, treg2
    RESET_PRU_CLOCK treg1, treg2
    MVIB    *r1.b0++, r31.b0
    QBNE    BIT_LOOP, r1.b0, 40 + BYTES_PER_STORE

    //got the number of bytes needed, store it
    LDI     treg1,  12000
    QBLT    DONE_DATA_STORE, lengthReg, treg1
        SBBO    &data_reg, smemLocReg, lengthReg, BYTES_PER_STORE
DONE_DATA_STORE
    ADD     lengthReg, lengthReg, BYTES_PER_STORE
    SBCO    &lengthReg, CONST_PRUDRAM, 4, 4

    //check for exit
    XIN     10, &enableReg, 8
    QBNE    IDLE_LOOP, enableReg, 1

    //reset counter
    LDI     r1.b0, 40
    //next set of bit
    JMP     BIT_LOOP

EXIT:
	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT
