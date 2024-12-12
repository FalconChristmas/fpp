#ifndef __falcon_utils__
#define __falcon_utils__

#include "FalconPRUDefs.hp"

#define PRU0_CONTROL_REG    0x00022000
#define PRU1_CONTROL_REG    0x00024000

#ifndef RUNNING_ON_PRU1
#define PRU_CONTROL_REG PRU0_CONTROL_REG
#define PRU_MEMORY_OFFSET 0
#define PRU_ARM_INTERRUPT PRU0_ARM_INTERRUPT
#else
#define PRU_CONTROL_REG PRU1_CONTROL_REG
#define PRU_MEMORY_OFFSET 0x2000
#define PRU_ARM_INTERRUPT PRU1_ARM_INTERRUPT
#endif

/* needs two temporary registers that can be cleared out */
RESET_PRU_CLOCK .macro reg1, reg2
    LDI32  reg2, PRU_CONTROL_REG
    LBBO   &reg1, reg2, 0, 4
    CLR    reg1, reg1, 3
    SBBO   &reg1, reg2, 0, 4
    LDI32  reg1, 0
    SBBO   &reg1, reg2, 0xC, 4
    LBBO   &reg1, reg2, 0, 4
    SET    reg1, reg1, 3
    SBBO   &reg1, reg2, 0, 4
    .endm

/* if size = 8, then the reg beyond the passed in will contain the stall count */
GET_PRU_CLOCK .macro  reg, treg, size
    LDI32 treg, PRU_CONTROL_REG
    LBBO  &reg, treg, 0xC, size
    .endm

/* Wait until the clock has incremented a given number of nanoseconds with 10 ns resolution.
   Based on the clock.   User RESET_PRU_CLOCK to set to 0 first. */
WAITNS .macro   ns, treg1, treg2
    .newblock
    LDI32 treg1, PRU_CONTROL_REG
waitloop?:
    LBBO &treg2, treg1, 0xC, 4 // read the cycle counter
    QBGT waitloop?, treg2, NS2CLK(ns)
    .endm

LONGWAITNS .macro   ns, treg1, treg2
    .newblock
waitloop?:
    LDI32 treg1, PRU_CONTROL_REG
    LBBO &treg2, treg1, 0xC, 4 // read the cycle counter
    LDI32 treg1, NS2CLK(ns)
    QBGT waitloop?, treg2, treg1
    .endm


/* Wait until the clock has incremented a given number of nanoseconds but use HW loop. */
/* Cannot be used within an existing harware loop (LOOP) */
/* Slightly more accurate than WAITNS and uses only one register, but cannot be used within a LOOP */
WAITNS_LOOP .macro  ns, treg1, treg2
    .newblock
    LDI32 treg1, PRU_CONTROL_REG
    LBBO &treg2, treg1, 0xC, 4
    MAX treg1, treg2, NS2CLK(ns)-2  // MAX and SUB are 2 cycles
    SUB treg1, treg1, treg2
    LOOP endloop?, treg1
        NOP
endloop?:
    .endm 

/* Busy sleep for the given number of ns */
SLEEPNS .macro  ns, treg, extra
       .newblock
       LDI treg, NS2CLK(ns / 2) - 1 - extra
sleeploop?:
       SUB treg, treg, 1
       QBNE sleeploop?, treg, 0
    .endm


#endif
