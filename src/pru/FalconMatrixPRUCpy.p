
#ifdef RUNNING_ON_PRU1
#undef RUNNING_ON_PRU1
#define RUNNING_ON_PRU0
#else
#undef RUNNING_ON_PRU0
#define RUNNING_ON_PRU1
#endif


.origin 0
.entrypoint START

#include "FalconUtils.hp"
#include "FalconPRUDefs.hp"

#define data_addr       r1

#define endVal          r7


#define lastData        r17
#define pixel_data      r18 // the next 8 registers, too;



START:
    // Enable OCP master port
    // clear the STANDBY_INIT bit in the SYSCFG register,
    // otherwise the PRU will not be able to write outside the
    // PRU memory space and to the BeagleBone's pins.
    LBCO    r0, C4, 4, 4
    CLR     r0, r0, 4
    SBCO    r0, C4, 4, 4

    // Configure the programmable pointer register for PRU0 by setting
    // c28_pointer[15:0] field to 0x0120.  This will make C28 point to
    // 0x00012000 (PRU shared RAM).
    MOV		r0, 0x00000120
    MOV		r1, CTPPR_0 + PRU_MEMORY_OFFSET
    ST32	r0, r1

    // Configure the programmable pointer register for PRU0 by setting
    // c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    // 0x80001000 (DDR memory).
    MOV		r0, 0x00100000
    MOV		r1, CTPPR_1 + PRU_MEMORY_OFFSET
    ST32	r0, r1

    LDI r3, 1
    SBCO r3, C24, 0, 4

    MOV endVal, 0xFFFFFFF
    MOV data_addr, 0
    ZERO &lastData, (32+4)

READ_LOOP:
    XIN 10, data_addr, 4

    // Wait for a non-zero address
    QBEQ READ_LOOP, data_addr, #0

    // Command of 0xFFFFFFF is the signal to exit
    QBEQ EXIT, data_addr, endVal

    QBNE DO_DATA, lastData, data_addr
    //nothing changed
    JMP READ_LOOP

    DO_DATA:
        LBBO pixel_data, data_addr, 0, 32
        MOV lastData, data_addr
        XOUT 11, lastData, (32 + 4)
        JMP READ_LOOP

EXIT:
    #ifdef AM33XX
        // Send notification to Host for program completion
        MOV R31.b0, PRU_ARM_INTERRUPT+16
    #else
        MOV R31.b0, PRU_ARM_INTERRUPT
    #endif

HALT


