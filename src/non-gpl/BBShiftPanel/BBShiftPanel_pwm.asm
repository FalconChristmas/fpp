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
#define curReg      r0.b1
#define numReg      r0.b2
#define flags		r0.b3
#define dataOutReg  r1.b0
#define curBlock    r1.b1
#define outputData  r2
#define xferR1      r18
#define xferR2      r19
#define xferR3      r20


// these can co-exist with xferR2/R3
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

    // Make sure variables and such are clear
    LDI     flags, 0
    LDI     xshiftReg, 0
  
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

    // Start pre-load of data
    PRELOAD_DATA

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

    UNPRELOAD_DATA
    JMP  _LOOP





OUTPUT_DATA:
    MOV   curRow, numRows
DOROWOUTPUT:
    QBEQ  DONEDATAOUT, curRow, 0
    LDI   curReg, 16

DOREGISTEROUTPUT:
    QBEQ  DONEREGISTEROUT, curReg, 0
    MOV   curBlock, numBlocks
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
    SUB curReg, curReg, 1
    JMP DOREGISTEROUTPUT
DONEREGISTEROUT:    
    SUB     curRow, curRow, 1
    JMP     DOROWOUTPUT
DONEDATAOUT:
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    OUTPUT_PIXEL_CLRLE
    JMP SKIPDATA



OUTPUT_REGISTERS:
    .newblock
    // Signal the GCLK to do 4 ticks
    LOW_FOR_CLOCKS 2
    LDI   r19, 1
    XOUT  12, &r19, 4
    LOW_FOR_CLOCKS 1

	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 7
	LE_FOR_CLOCKS_NO_CLR 12
    LOW_FOR_CLOCKS 6

    //-----------
    // Do register #1
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG1_LOOP:
	LBCO	&r2, CONST_PRUDRAM, 16, 48    
    QBEQ    REG1_LAST, curReg,  1
    DO_FULL_REGISTER    16
    SUB     curReg, curReg, 1
    JMP     REG1_LOOP
REG1_LAST:
    DO_REG1_LAST        16
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #2
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG2_LOOP:
	LBCO	&r2, CONST_PRUDRAM, 112, 48    
    QBEQ    REG2_LAST, curReg,  1
    DO_FULL_REGISTER    112
    SUB     curReg, curReg, 1
    JMP     REG2_LOOP
REG2_LAST:
    DO_REG2_LAST        112
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #3
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG3_LOOP:
    LDI     tmpReg1, 208
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48    
    QBEQ    REG3_LAST, curReg,  1
    DO_FULL_REGISTER    208
    SUB     curReg, curReg, 1
    JMP     REG3_LOOP
REG3_LAST:
    DO_REG3_LAST        208
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #4
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG4_LOOP:
    LDI     tmpReg1, 304
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48    
    QBEQ    REG4_LAST, curReg,  1
    DO_FULL_REGISTER    304
    SUB     curReg, curReg, 1
    JMP     REG4_LOOP
REG4_LAST:
    DO_REG4_LAST        304
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6
    //-----------


    //-----------
    // Do register #5
    MOV     curReg, numBlocks
	LE_FOR_CLOCKS_NO_CLR 14
    LOW_FOR_CLOCKS 4
REG5_LOOP:
    LDI     tmpReg1, 400
	LBCO	&r2, CONST_PRUDRAM, tmpReg1, 48    
    QBEQ    REG5_LAST, curReg,  1
    DO_FULL_REGISTER    400
    SUB     curReg, curReg, 1
    JMP     REG5_LOOP
REG5_LAST:
    DO_REG5_LAST        400
    CLEAR_DATA_PINS
    LOW_FOR_CLOCKS 6

    JMP SKIPREGISTERS








EXIT:
    LDI     tmpReg1, 0xFF
    XOUT    12, &tmpReg1, 4

	// Send notification to Host for program completion
	LDI R31.b0, PRU_ARM_INTERRUPT+16
	HALT



