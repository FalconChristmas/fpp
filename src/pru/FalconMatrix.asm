/*
 *PRU based LED Matrix driver.
 */
 #include "ResourceTable.asm"
 
#ifndef OUTPUTS
#define OUTPUTS 8
#endif

#ifndef RUNNING_ON_PRU1
#ifndef RUNNING_ON_PRU0
#define RUNNING_ON_PRU0
#endif
#endif

#define CAT3(X,Y,Z) X##Y##Z

#include "FalconPRUDefs.hp"
#include "FalconUtils.asm"

/** Register map */
#define data_addr       r1
                        //  r2.w0 ??
#define row             r2.b2
#define bright          r2.b3

#define gpio0_led_mask  r3      // led masks for each of the GPIOs
#define gpio1_led_mask  r4
#define gpio2_led_mask  r5
#define gpio3_led_mask  r6

#define sleep_counter   r7.w0
#define sleepDone       r7.w2
#define statOffset      r8.w0
#define pixelCount      r8.w2

#define gpio_base_cache r9      // register to keep a base address cached.  handy if its used for clocks

#define tmp_reg1        r10
#define tmp_reg2        r11
#define tmp_reg3        r12
#define tmp_reg4        r13
                        //r15-r16  ??

#define data_from_other_pru   r17   // the address that the other PRU loaded into r18+
#define pixel_data        r18       // the next 8 registers, too;
#define last_pixel_data_0 r26
#define last_pixel_data_1 r27
#define last_pixel_data_2 r28
#define last_pixel_data_3 r29


#ifdef GPIO_CLR_FIRST
#define gpio_base       tmp_reg1    // must be one less than out_clr
#define out_clr         tmp_reg2    // must be one less than out_set
#define out_set         tmp_reg3
#define out_clrset      out_clr
#define 
#else
#define gpio_base       tmp_reg1    // must be one less than out_set
#define out_set         tmp_reg2    // must be one less than out_clr
#define out_clr         tmp_reg3
#define out_clrset      out_set
#endif


#define GPIO(R) CAT3(gpio,R,_set)


#ifdef USING_PWM
//nothing to do, PWM hardware handles it
#define CHECK_FOR_DISPLAY_OFF
#else
CHECK_FOR_DISPLAY_OFF .macro
        .newblock
        QBEQ END?, sleep_counter, 0
        GET_PRU_CLOCK tmp_reg1, tmp_reg2, 4
        QBGT END?, tmp_reg1, sleep_counter
        MOV sleepDone, tmp_reg1
        DISPLAY_OFF
        LDI sleep_counter, 0
END?:
    .endm
#endif



OUTPUT_GPIO .macro data, mask, gpio, last
    .newblock
    ; We write 8 bytes since CLR and DATA are contiguous,
    ; which will write both the 0 and 1 bits in the
    ; same instruction.
    QBEQ DONEOUTPUTGPIO?, data, last
    MOV last, data
    LDI32 gpio_base, gpio
    QBEQ SETALL?, data, mask
    QBEQ CLEARALL?, data, 0
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO &out_clrset, gpio_base, GPIO_SETCLRDATAOUT, 8
        QBA DONEOUTPUTGPIO?
SETALL?:
        SBBO &data, gpio_base, GPIO_SETDATAOUT, 4
        QBA DONEOUTPUTGPIO?
CLEARALL?:
        SBBO &mask, gpio_base, GPIO_CLRDATAOUT, 4
DONEOUTPUTGPIO?:
    .endm


OUTPUT_GPIO_FORCE_CLEAR .macro data, mask, gpio, last
    .newblock
    LDI32 gpio_base, gpio
    ; We write 8 bytes since CLR and DATA are contiguous,
    ; which will write both the 0 and 1 bits in the
    ; same instruction.
    QBEQ CLEARALL?, data, 0
        QBEQ CLEARNONDATA?, data, last
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO &out_clrset, gpio_base, GPIO_SETCLRDATAOUT, 8
        QBA DONEOUTPUTGPIO?
CLEARALL?:
        SBBO &mask, gpio_base, GPIO_CLRDATAOUT, 4
        QBA DONEOUTPUTGPIO?
CLEARNONDATA?:
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO &out_clr, gpio_base, GPIO_CLRDATAOUT, 4
DONEOUTPUTGPIO?:
    MOV last, data
    CHECK_FOR_DISPLAY_OFF
    .endm


#include "/tmp/PanelPinConfiguration.hp"
#include "FalconMatrixCommon.asm"


OUTPUT_GPIOS .macro  d0, d1, d2, d3
    CHECK_FOR_DISPLAY_OFF
    OUTPUT_GPIO_0(d0, gpio0_led_mask, GPIO0, last_pixel_data_0)
    OUTPUT_GPIO_1(d1, gpio1_led_mask, GPIO1, last_pixel_data_1)
    OUTPUT_GPIO_2(d2, gpio2_led_mask, GPIO2, last_pixel_data_2)
    OUTPUT_GPIO_3(d3, gpio3_led_mask, GPIO3, last_pixel_data_3)
    .endm



#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)
#define CONFIGURE_PIN(a) SET GPIO_MASK(a##_gpio), GPIO_MASK(a##_gpio), a##_pin

#ifdef AM33XX
DISABLE_GPIO_PIN_INTERRUPTS .macro ledMask, gpio
    MOV tmp_reg1, ledMask
    MOV tmp_reg2, ledMask
    LDI32 tmp_reg3, gpio
    LDI tmp_reg4, 0x100
    SUB tmp_reg3, tmp_reg3, tmp_reg4
    SBBO &tmp_reg1, tmp_reg3, 0x3C, 8
            ; 0x3c is the GPIO_IRQSTATUS_CLR_0 register
            ; by doing 8 and using both tmp_reg1 and tmp_reg2, we can clear
            ; both the 0 and 1 IRQ status
    .endm
    
DISABLE_PIN_INTERRUPTS .macro
    DISABLE_GPIO_PIN_INTERRUPTS gpio0_led_mask, GPIO0
    DISABLE_GPIO_PIN_INTERRUPTS gpio1_led_mask, GPIO1
    DISABLE_GPIO_PIN_INTERRUPTS gpio2_led_mask, GPIO2
    DISABLE_GPIO_PIN_INTERRUPTS gpio3_led_mask, GPIO3

    ZERO &tmp_reg4, 4
    #ifdef gpio_sel0
        SET tmp_reg4, tmp_reg4, gpio_sel0
        SET tmp_reg4, tmp_reg4, gpio_sel1
        SET tmp_reg4, tmp_reg4, gpio_sel2
        SET tmp_reg4, tmp_reg4, gpio_sel3
    #endif
    #ifdef gpio_clock
        SET tmp_reg4, tmp_reg4, gpio_clock
    #endif
    #ifdef gpio_oe
        SET tmp_reg4, tmp_reg4, gpio_oe
    #endif
    #ifdef gpio_latch
        SET tmp_reg4, tmp_reg4, gpio_latch
    #endif
    DISABLE_GPIO_PIN_INTERRUPTS tmp_reg4, CONTROLS_GPIO_BASE
    .endm
#else    
DISABLE_PIN_INTERRUPTS .macro
    .endm
#endif            


GET_ON_DELAY_TIMES .macro brightlevel
#ifdef USING_PWM
    LSL tmp_reg1, brightlevel, 1
    ADD tmp_reg1, tmp_reg1, 10
    LBCO tmp_reg1, CONST_PRUDRAM, tmp_reg1, 2
    LDI tmp_reg2, 0
#else
#ifdef BRIGHTNESS12
    QBEQ DO12, brightlevel, 12
#endif
#ifdef BRIGHTNESS11
    QBEQ DO11, brightlevel, 11
#endif
#ifdef BRIGHTNESS10
    QBEQ DO10, brightlevel, 10
#endif
#ifdef BRIGHTNESS9
    QBEQ DO9, brightlevel, 9
#endif
#ifdef BRIGHTNESS8
    QBEQ DO8, brightlevel, 8
#endif
#ifdef BRIGHTNESS7
    QBEQ DO7, brightlevel, 7
#endif
    QBEQ DO6, brightlevel, 6
    QBEQ DO5, brightlevel, 5
    QBEQ DO4, brightlevel, 4
    QBEQ DO3, brightlevel, 3
    QBEQ DO2, brightlevel, 2

    LDI32or16 tmp_reg1, BRIGHTNESS1
    LDI32or16 tmp_reg2, DELAY1

    QBA DONETIMES
#ifdef BRIGHTNESS12
DO12:
        LDI32or16 tmp_reg1, BRIGHTNESS12
        LDI32or16 tmp_reg2, DELAY12
        QBA DONETIMES
#endif
#ifdef BRIGHTNESS11
DO11:
        LDI32or16 tmp_reg1, BRIGHTNESS11
        LDI32or16 tmp_reg2, DELAY11
        QBA DONETIMES
#endif
#ifdef BRIGHTNESS10
DO10:
        LDI32or16 tmp_reg1, BRIGHTNESS10
        LDI32or16 tmp_reg2, DELAY10
        QBA DONETIMES
#endif
#ifdef BRIGHTNESS9
DO9:
        LDI32or16 tmp_reg1, BRIGHTNESS9
        LDI32or16 tmp_reg2, DELAY9
        QBA DONETIMES
#endif
#ifdef BRIGHTNESS8
DO8:
        LDI32or16 tmp_reg1, BRIGHTNESS8
        LDI32or16 tmp_reg2, DELAY8
        QBA DONETIMES
#endif
#ifdef BRIGHTNESS7
DO7:
        LDI32or16 tmp_reg1, BRIGHTNESS7
        LDI32or16 tmp_reg2, DELAY7
        QBA DONETIMES
#endif
DO6:
        LDI32or16 tmp_reg1, BRIGHTNESS6
        LDI32or16 tmp_reg2, DELAY6
        QBA DONETIMES
DO5:
        LDI32or16 tmp_reg1, BRIGHTNESS5
        LDI32or16 tmp_reg2, DELAY5
        QBA DONETIMES
DO4:
        LDI32or16 tmp_reg1, BRIGHTNESS4
        LDI32or16 tmp_reg2, DELAY4
        QBA DONETIMES
DO3:
        LDI32or16 tmp_reg1, BRIGHTNESS3
        LDI32or16 tmp_reg2, DELAY3
        QBA DONETIMES
DO2:
        LDI32or16 tmp_reg1, BRIGHTNESS2
        LDI32or16 tmp_reg2, DELAY2
        QBA DONETIMES
DONETIMES:
#endif
    .endm

;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
    .sect    ".text:main"
    .clink
    .global    ||main||

||main||:
#ifdef AM33XX    
    ; Enable OCP master port
    ; clear the STANDBY_INIT bit in the SYSCFG register,
    ; otherwise the PRU will not be able to write outside the
    ; PRU memory space and to the BeagleBones pins.
    LBCO	&r0, C4, 4, 4
    CLR		r0, r0, 4
    SBCO	&r0, C4, 4, 4
#endif

    ; Configure the programmable pointer register for PRU0 by setting
    ; c28_pointer[15:0] field to 0x0120.  This will make C28 point to
    ; 0x00012000 (PRU shared RAM).
    LDI 	r0, 0x120
    LDI32	r1, CTPPR_0 + PRU_MEMORY_OFFSET
    SBBO    &r0, r1, 0x00, 4

    ; Configure the programmable pointer register for PRU0 by setting
    ; c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    ; 0x80001000 (DDR memory).
    LDI32	r0, 0x00100000
    LDI32	r1, CTPPR_1 + PRU_MEMORY_OFFSET
    SBBO    &r0, r1, 0x00, 4

    ; Write a 0x1 into the response field so that they know we have started
    LDI     r2, 0x1
    SBCO    &r2, CONST_PRUDRAM, 8, 4


    LDI gpio0_led_mask, 0
    LDI gpio1_led_mask, 0
    LDI gpio2_led_mask, 0
    LDI gpio3_led_mask, 0

#ifndef NO_OUTPUT_1
    CONFIGURE_PIN(r11)
    CONFIGURE_PIN(g11)
    CONFIGURE_PIN(b11)
    CONFIGURE_PIN(r12)
    CONFIGURE_PIN(g12)
    CONFIGURE_PIN(b12)
#endif
#ifndef NO_OUTPUT_2
    CONFIGURE_PIN(r21)
    CONFIGURE_PIN(g21)
    CONFIGURE_PIN(b21)
    CONFIGURE_PIN(r22)
    CONFIGURE_PIN(g22)
    CONFIGURE_PIN(b22)
#endif
#ifndef NO_OUTPUT_3
    CONFIGURE_PIN(r31)
    CONFIGURE_PIN(g31)
    CONFIGURE_PIN(b31)
    CONFIGURE_PIN(r32)
    CONFIGURE_PIN(g32)
    CONFIGURE_PIN(b32)
#endif
#ifndef NO_OUTPUT_4
    CONFIGURE_PIN(r41)
    CONFIGURE_PIN(g41)
    CONFIGURE_PIN(b41)
    CONFIGURE_PIN(r42)
    CONFIGURE_PIN(g42)
    CONFIGURE_PIN(b42)
#endif
#ifndef NO_OUTPUT_5
    CONFIGURE_PIN(r51)
    CONFIGURE_PIN(g51)
    CONFIGURE_PIN(b51)
    CONFIGURE_PIN(r52)
    CONFIGURE_PIN(g52)
    CONFIGURE_PIN(b52)
#endif
#ifndef NO_OUTPUT_6
    CONFIGURE_PIN(r61)
    CONFIGURE_PIN(g61)
    CONFIGURE_PIN(b61)
    CONFIGURE_PIN(r62)
    CONFIGURE_PIN(g62)
    CONFIGURE_PIN(b62)
#endif
#ifndef NO_OUTPUT_7
    CONFIGURE_PIN(r71)
    CONFIGURE_PIN(g71)
    CONFIGURE_PIN(b71)
    CONFIGURE_PIN(r72)
    CONFIGURE_PIN(g72)
    CONFIGURE_PIN(b72)
#endif
#ifndef NO_OUTPUT_8
    CONFIGURE_PIN(r81)
    CONFIGURE_PIN(g81)
    CONFIGURE_PIN(b81)
    CONFIGURE_PIN(r82)
    CONFIGURE_PIN(g82)
    CONFIGURE_PIN(b82)
#endif


    ADJUST_SETTINGS

    DISABLE_PIN_INTERRUPTS

#ifdef USING_PWM
    //We need to record the Period in clock cycles so that
    //The brightness values can be calculated
    MOV data_addr, oe_pwm_address
    LBBO &pixel_data, &data_addr, 0xA, 2
    SBCO &pixel_data, CONST_PRUDRAM, 12, 2
#endif

    LDI sleep_counter, 0
    LDI sleepDone, 0

    RESET_PRU_CLOCK tmp_reg1, tmp_reg2
    LDI statOffset, 0

    //zero out data
    LDI32 data_addr, 0
    LDI32 r2, 0
    SBCO &data_addr, CONST_PRUDRAM, 0, 8

    // make sure the display is off
    DISPLAY_OFF
READ_LOOP:
    ; Load the pointer to the buffer from PRU DRAM into r1
    ; command into r2
    LBCO &data_addr, CONST_PRUDRAM, 0, 8

    ; Wait for a non-zero command
    QBEQ READ_LOOP, r2, 0

    ; Command of 0xFF is the signal to exit
    QBNE NO_EXIT, r2, 0xFF
        QBA EXIT

NO_EXIT:
    LDI row, 0
    LDI statOffset, 0
    XOUT 10, &data_addr, 4


#ifdef OUTPUTBYROW
#include "FalconMatrixByRow.asm"
#else
#include "FalconMatrixByDepth.asm"
#endif

EXIT:
    LDI32 data_addr, 0xFFFFFFF
    XOUT 10, &data_addr, 4
    ; Send notification to Host for program completion
    LDI R31.b0, PRU_ARM_INTERRUPT+16
    HALT
