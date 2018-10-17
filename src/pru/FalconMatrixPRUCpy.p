
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
#include "FalconWS281x.hp"

#define data_addr       r1
#define endVal          r2

#define lastData        r17
#define pixel_data      r18 // the next 12 registers, too;



START:
    MOV endVal, 0xFFFFFFF
    MOV data_addr, 0
    ZERO &lastData, 32

    LDI r3, 1
    SBCO r3, C24, 0, 4
    SBCO lastData, C24, 4, 32

READ_LOOP:
    XIN 10, data_addr, 4

    // Wait for a non-zero address
    QBEQ READ_LOOP, data_addr, #0

    // Command of 0xFFFFFFF is the signal to exit
    QBEQ EXIT, data_addr, endVal

    QBNE DO_DATA, lastData, data_addr
    //nothing changed
    JMP READ_LOOP

    SBCO data_addr, C24, 4, 4


    DO_DATA:
        LBBO pixel_data, data_addr, 0, 3*2*OUTPUTS
        MOV lastData, data_addr
        XOUT 11, lastData, (3*2*OUTPUTS + 4)
        SBCO lastData, C24, 8, (3*2*OUTPUTS + 4)

    JMP READ_LOOP

EXIT:
    #ifdef AM33XX
        // Send notification to Host for program completion
        MOV R31.b0, PRU_ARM_INTERRUPT+16
    #else
        MOV R31.b0, PRU_ARM_INTERRUPT
    #endif

HALT


