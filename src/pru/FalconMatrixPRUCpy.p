

#undef RUNNING_ON_PRU1
#undef RUNNING_ON_PRU0
#define RUNNING_ON_PRU1


.origin 0
.entrypoint START

#include "FalconUtils.hp"
#include "FalconWS281x.hp"

#define data_addr       r0
#define offset          r1

#define endVal          r2

#define lastData        r8

#define lastOffset      r17
#define pixel_data      r18 // the next 12 registers, too;



START:
    MOV endVal, 0xFFFFFFF
    MOV lastOffset, 0xFFFFFF
    MOV data_addr, 0

READ_LOOP:
    XIN 10, data_addr, 8

    // Wait for a non-zero address
    QBEQ READ_LOOP, data_addr, #0

    // Command of 0xFFFFFF is the signal to exit
    QBEQ EXIT, data_addr, endVal

    QBNE DO_DATA, lastData, data_addr
    QBNE DO_DATA, lastOffset, offset
    //nothing changed
    JMP READ_LOOP

    DO_DATA:
        LBBO pixel_data, data_addr, offset, 48
        MOV lastOffset, offset
        XOUT 11, lastOffset, 52
        MOV lastData, data_addr


    JMP READ_LOOP

EXIT:
    #ifdef AM33XX
        // Send notification to Host for program completion
        MOV R31.b0, PRU_ARM_INTERRUPT+16
    #else
        MOV R31.b0, PRU_ARM_INTERRUPT
    #endif

HALT


