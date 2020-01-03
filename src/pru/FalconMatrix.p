/*
 *PRU based LED Matrix driver.
 */
#ifndef OUTPUTS
#define OUTPUTS 8
#endif

#ifndef RUNNING_ON_PRU1
#ifndef RUNNING_ON_PRU0
#define RUNNING_ON_PRU0
#endif
#endif

#define CAT3(X,Y,Z) X##Y##Z


.origin 0
.entrypoint START

#include "FalconPRUDefs.hp"
#include "FalconUtils.hp"


/** Register map */
#define data_addr       r1
                        //  r2.w0 ??
#define row             r2.b2
#define bright          r2.b3

#define gpio0_led_mask  r3      // led masks for each of the GPIO's
#define gpio1_led_mask  r4
#define gpio2_led_mask  r5
#define gpio3_led_mask  r6

#define sleep_counter   r7.w0
#define sleepDone       r7.w2
#define statOffset      r8.w0
#define pixelCount      r8.w2

#define gpio_base_cache r9      // register to keep a base address cached.  handy if it's used for clocks

#define tmp_reg1        r10
#define tmp_reg2        r11
#define tmp_reg3        r12
#define tmp_reg4        r13
                        //r15-r16  ??

#define data_from_other_pru   r17   // the address that the other PRU loaded into r18+
#define pixel_data        r18 // the next 8 registers, too;
#define last_pixel_data_0 r26
#define last_pixel_data_1 r27
#define last_pixel_data_2 r28
#define last_pixel_data_3 r29


#define gpio_base       tmp_reg1 // must be one less than out_clr
#define out_clr         tmp_reg2 // must be one less than out_set
#define out_set         tmp_reg3




#define GPIO(R) CAT3(gpio,R,_set)


#ifdef USING_PWM
//nothing to do, PWM hardware handles it
#define CHECK_FOR_DISPLAY_OFF
#else
.macro CHECK_FOR_DISPLAY_OFF
.mparam reg1 = tmp_reg1, reg2 = tmp_reg2
    QBEQ NO_BLANK, sleep_counter, 0
        GET_PRU_CLOCK reg1, reg2
        QBGT NO_BLANK, reg1, sleep_counter
        MOV sleepDone, reg1
        DISPLAY_OFF
        LDI sleep_counter, 0
    NO_BLANK:
.endm
#endif



.macro OUTPUT_GPIO
.mparam data, mask, gpio, last
    // We write 8 bytes since CLR and DATA are contiguous,
    // which will write both the 0 and 1 bits in the
    // same instruction.
    QBEQ DONEOUTPUTGPIO, data, last
    MOV last, data
    MOV gpio_base, gpio
    QBEQ SETALL, data, mask
    QBEQ CLEARALL, data, 0
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 8
        JMP DONEOUTPUTGPIO
    SETALL:
        SBBO data, gpio_base, GPIO_SETDATAOUT, 4
        JMP DONEOUTPUTGPIO
    CLEARALL:
        SBBO mask, gpio_base, GPIO_CLRDATAOUT, 4
    DONEOUTPUTGPIO:
.endm


.macro OUTPUT_GPIO_FORCE_CLEAR
.mparam data, mask, gpio, last
    MOV gpio_base, gpio
    // We write 8 bytes since CLR and DATA are contiguous,
    // which will write both the 0 and 1 bits in the
    // same instruction.
    QBEQ CLEARALL, data, 0
        QBEQ CLEARNONDATA, data, last
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 8
        JMP DONEOUTPUTGPIO
    CLEARALL:
        SBBO mask, gpio_base, GPIO_CLRDATAOUT, 4
        JMP DONEOUTPUTGPIO
    CLEARNONDATA:
        AND out_set, data, mask
        XOR out_clr, out_set, mask
        SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 4
    DONEOUTPUTGPIO:
    MOV last, data
    CHECK_FOR_DISPLAY_OFF
.endm


#include "/tmp/PanelPinConfiguration.hp"
#include "FalconMatrixCommon.p"


.macro OUTPUT_GPIOS
.mparam d0, d1, d2, d3
    CHECK_FOR_DISPLAY_OFF
    OUTPUT_GPIO_0(d0, gpio0_led_mask, GPIO0, last_pixel_data_0)
    OUTPUT_GPIO_1(d1, gpio1_led_mask, GPIO1, last_pixel_data_1)
    OUTPUT_GPIO_2(d2, gpio2_led_mask, GPIO2, last_pixel_data_2)
    OUTPUT_GPIO_3(d3, gpio3_led_mask, GPIO3, last_pixel_data_3)
.endm



#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)
#define CONFIGURE_PIN(a) SET GPIO_MASK(a##_gpio), a##_pin

.macro DISABLE_GPIO_PIN_INTERRUPTS
.mparam ledMask, gpio
    MOV tmp_reg1, ledMask
    MOV tmp_reg2, ledMask
    MOV tmp_reg3, gpio
    MOV tmp_reg4, 0x100
    SUB tmp_reg3, tmp_reg3, tmp_reg4
    SBBO tmp_reg1, tmp_reg3, 0x3C, 8
            //0x3c is the GPIO_IRQSTATUS_CLR_0 register
            // by doing 8 and using both tmp_reg1 and tmp_reg2, we can clear
            // both the 0 and 1 IRQ status
.endm
.macro DISABLE_PIN_INTERRUPTS
    DISABLE_GPIO_PIN_INTERRUPTS gpio0_led_mask, GPIO0
    DISABLE_GPIO_PIN_INTERRUPTS gpio1_led_mask, GPIO1
    DISABLE_GPIO_PIN_INTERRUPTS gpio2_led_mask, GPIO2
    DISABLE_GPIO_PIN_INTERRUPTS gpio3_led_mask, GPIO3

    ZERO &tmp_reg4, 4
    #ifdef gpio_sel0
        SET tmp_reg4, gpio_sel0
        SET tmp_reg4, gpio_sel1
        SET tmp_reg4, gpio_sel2
        SET tmp_reg4, gpio_sel3
    #endif
    #ifdef gpio_clock
        SET tmp_reg4, gpio_clock
    #endif
    #ifdef gpio_oe
        SET tmp_reg4, gpio_oe
    #endif
    #ifdef gpio_latch
        SET tmp_reg4, gpio_latch
    #endif
    DISABLE_GPIO_PIN_INTERRUPTS tmp_reg4, gpio_base_cache
.endm

.macro GET_ON_DELAY_TIMES
.mparam brightlevel
#ifdef USING_PWM
    LSL tmp_reg1, brightlevel, 1
    ADD tmp_reg1, tmp_reg1, 10
    LBCO tmp_reg1, CONST_PRUDRAM, tmp_reg1, 2
    MOV tmp_reg2, 0
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

    MOV tmp_reg1, BRIGHTNESS1
    MOV tmp_reg2, DELAY1

    JMP DONETIMES
#ifdef BRIGHTNESS12
    DO12:
        MOV tmp_reg1, BRIGHTNESS12
        MOV tmp_reg2, DELAY12
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS11
    DO11:
        MOV tmp_reg1, BRIGHTNESS11
        MOV tmp_reg2, DELAY11
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS10
    DO10:
        MOV tmp_reg1, BRIGHTNESS10
        MOV tmp_reg2, DELAY10
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS9
    DO9:
        MOV tmp_reg1, BRIGHTNESS9
        MOV tmp_reg2, DELAY9
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS8
    DO8:
        MOV tmp_reg1, BRIGHTNESS8
        MOV tmp_reg2, DELAY8
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS7
    DO7:
        MOV tmp_reg1, BRIGHTNESS7
        MOV tmp_reg2, DELAY7
        JMP DONETIMES
#endif
    DO6:
        MOV tmp_reg1, BRIGHTNESS6
        MOV tmp_reg2, DELAY6
        JMP DONETIMES
    DO5:
        MOV tmp_reg1, BRIGHTNESS5
        MOV tmp_reg2, DELAY5
        JMP DONETIMES
    DO4:
        MOV tmp_reg1, BRIGHTNESS4
        MOV tmp_reg2, DELAY4
        JMP DONETIMES
    DO3:
        MOV tmp_reg1, BRIGHTNESS3
        MOV tmp_reg2, DELAY3
        JMP DONETIMES
    DO2:
        MOV tmp_reg1, BRIGHTNESS2
        MOV tmp_reg2, DELAY2
        JMP DONETIMES

    DONETIMES:
#endif
.endm


START:
    // Enable OCP master port
    // clear the STANDBY_INIT bit in the SYSCFG register,
    // otherwise the PRU will not be able to write outside the
    // PRU memory space and to the BeagleBon's pins.
    LBCO	r0, C4, 4, 4
    CLR		r0, r0, 4
    SBCO	r0, C4, 4, 4

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

    // Write a 0x1 into the response field so that they know we have started
    MOV r2, #0x1
    SBCO r2, CONST_PRUDRAM, 8, 4


    MOV gpio0_led_mask, 0
    MOV gpio1_led_mask, 0
    MOV gpio2_led_mask, 0
    MOV gpio3_led_mask, 0


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
    LBBO pixel_data, data_addr, 0xA, 2
    SBCO pixel_data, CONST_PRUDRAM, 12, 2
#endif

    LDI sleep_counter, 0
    LDI sleepDone, 0

    RESET_PRU_CLOCK tmp_reg1, tmp_reg2
    LDI statOffset, 0

READ_LOOP:


    // Load the pointer to the buffer from PRU DRAM into r1
    // command into r2
    LBCO data_addr, CONST_PRUDRAM, 0, 8

    // Wait for a non-zero command
    QBEQ READ_LOOP, r2, #0

    // Command of 0xFF is the signal to exit
    QBNE NO_EXIT, r2, #0xFF
        JMP EXIT
NO_EXIT:

    MOV row, 0
    LDI statOffset, 0
    XOUT 10, data_addr, 4


#ifdef OUTPUTBYROW
#include "FalconMatrixByRow.p"
#else
#include "FalconMatrixByDepth.p"
#endif


EXIT:
    MOV data_addr, 0xFFFFFFF
    XOUT 10, data_addr, 4
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU_ARM_INTERRUPT
#endif

    HALT
