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

/** Mappings of the GPIO devices */
#define GPIO0 (0x44E07000 + 0x100)
#define GPIO1 (0x4804c000 + 0x100)
#define GPIO2 (0x481AC000 + 0x100)
#define GPIO3 (0x481AE000 + 0x100)

/** Offsets for the clear and set registers in the devices.
 * Since the offsets can only be 0xFF, we deliberately add offsets
 */
#define GPIO_DATAOUT    (0x13C - 0x100)
#define GPIO_CLRDATAOUT (0x190 - 0x100)
#define GPIO_SETDATAOUT (0x194 - 0x100)

/** Register map */
#define data_addr       r1
#define row             r2.b0
#define bright          r2.b1
#define sleep_counter   r3.w0
#define sleepDone       r3.w2
#define statOffset      r4.w0
#define pixelCount      r4.w2
#define out_clr r6 // must be one less than out_set
#define out_set r7
#define gpio0_set r8
#define gpio1_set r9
#define gpio2_set r10
#define gpio3_set r11
#define gpio0_led_mask r12
#define gpio1_led_mask r13
#define gpio2_led_mask r14
#define gpio3_led_mask r15
#define gpio_base_cache r16    // register to keep a base address cached.  handy if it's used for clocks
#define gpio_base       r17
#define pixel_data      r18 // the next 12 registers, too;



#define GPIO(R) CAT3(gpio,R,_set)

.macro DO_OUTPUT_ROW
.mparam rgpio, rpin, reg, bit
    QBBC skip, reg, bit
    SET rgpio, rpin
    skip:
.endm

#define OUTPUT_ROW(N,reg_r,reg_g,reg_b,bit) \
    DO_OUTPUT_ROW GPIO(r##N##_gpio), r##N##_pin, reg_r, bit; \
    DO_OUTPUT_ROW GPIO(g##N##_gpio), g##N##_pin, reg_g, bit; \
    DO_OUTPUT_ROW GPIO(b##N##_gpio), b##N##_pin, reg_b, bit; \


.macro CHECK_FOR_DISPLAY_OFF
.mparam reg1 = gpio0_set, reg2 = gpio1_set
    QBEQ NO_BLANK, sleep_counter, 0
        GET_PRU_CLOCK reg1, reg2
        QBGT NO_BLANK, reg1, sleep_counter
        MOV sleepDone, reg1
        DISPLAY_OFF
        LDI sleep_counter, 0
    NO_BLANK:
.endm



.macro OUTPUT_GPIO_WITH_BASE_REG
.mparam data, mask, gpio_base_reg
    // We write 8 bytes since CLR and DATA are contiguous,
    // which will write both the 0 and 1 bits in the
    // same instruction.

    AND out_set, data, mask
    XOR out_clr, out_set, mask
    SBBO out_clr, gpio_base_reg, GPIO_CLRDATAOUT, 8
.endm

.macro OUTPUT_GPIO
.mparam data, mask, gpio
    MOV gpio_base, gpio
    OUTPUT_GPIO_WITH_BASE_REG data, mask, gpio_base
.endm

#if defined OCTO_V1
#include "OctoscrollerV1.hp"
#include "OctoscrollerCommon.hp"
#elif defined OCTO_V2
#include "OctoscrollerV2.hp"
#include "OctoscrollerCommon.hp"
#elif defined POCKETSCROLLER_V1
#include "PocketScrollerV1.hp"
#else
#include "OctoscrollerV1.hp"
#include "OctoscrollerCommon.hp"
#endif



#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)
#define CONFIGURE_PIN(a) SET GPIO_MASK(a##_gpio), a##_pin
#define CONFIGURE_OUTPUT(a) CONFIGURE_PIN( r##a##1 ) ; \
    CONFIGURE_PIN( g##a##1 ) ; \
    CONFIGURE_PIN( b##a##1 ) ; \
    CONFIGURE_PIN( r##a##2 ) ; \
    CONFIGURE_PIN( g##a##2 ) ; \
    CONFIGURE_PIN( b##a##2 ) ;


.macro DISABLE_GPIO_PIN_INTERRUPTS
.mparam ledMask, gpio
    MOV gpio0_set, ledMask
    MOV gpio1_set, ledMask
    MOV gpio2_set, gpio
    MOV gpio3_set, 0x100
    SUB gpio2_set, gpio2_set, gpio3_set
    SBBO gpio0_set, gpio2_set, 0x3C, 8    //0x3c is the GPIO_IRQSTATUS_CLR_0 register
            // by doing 8 and using both gpio0_set and gpio1_set, we can clear
            // both the 0 and 1 IRQ status
.endm
.macro DISABLE_PIN_INTERRUPTS
    DISABLE_GPIO_PIN_INTERRUPTS gpio0_led_mask, GPIO0
    DISABLE_GPIO_PIN_INTERRUPTS gpio1_led_mask, GPIO1
    DISABLE_GPIO_PIN_INTERRUPTS gpio2_led_mask, GPIO2
    DISABLE_GPIO_PIN_INTERRUPTS gpio3_led_mask, GPIO3

    ZERO &gpio3_set, 4
    SET gpio3_set, gpio_sel0
    SET gpio3_set, gpio_sel1
    SET gpio3_set, gpio_sel2
    SET gpio3_set, gpio_sel3
    #ifdef gpio_clock
        SET gpio3_set, gpio_clock
    #endif
    #ifdef gpio_oe
        SET gpio3_set, gpio_oe
    #endif
    #ifdef gpio_latch
        SET gpio3_set, gpio_latch
    #endif
    DISABLE_GPIO_PIN_INTERRUPTS gpio3_set, gpio_base_cache
.endm

.macro GET_ON_DELAY_TIMES
.mparam brightlevel
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

    MOV gpio0_set, BRIGHTNESS1
    MOV gpio1_set, DELAY1

    JMP DONETIMES
#ifdef BRIGHTNESS8
    DO8:
        MOV gpio0_set, BRIGHTNESS8
        MOV gpio1_set, DELAY8
        JMP DONETIMES
#endif
#ifdef BRIGHTNESS7
    DO7:
        MOV gpio0_set, BRIGHTNESS7
        MOV gpio1_set, DELAY7
        JMP DONETIMES
#endif
    DO6:
        MOV gpio0_set, BRIGHTNESS6
        MOV gpio1_set, DELAY6
        JMP DONETIMES
    DO5:
        MOV gpio0_set, BRIGHTNESS5
        MOV gpio1_set, DELAY5
        JMP DONETIMES
    DO4:
        MOV gpio0_set, BRIGHTNESS4
        MOV gpio1_set, DELAY4
        JMP DONETIMES
    DO3:
        MOV gpio0_set, BRIGHTNESS3
        MOV gpio1_set, DELAY3
        JMP DONETIMES
    DO2:
        MOV gpio0_set, BRIGHTNESS2
        MOV gpio1_set, DELAY2
        JMP DONETIMES

    DONETIMES:
.endm


.macro OUTPUT_ROWS_FOR_BIT
.mparam bit
    ZERO &gpio0_set, 16

    OUTPUT_ROW(11, r18.b0, r18.b1, r18.b2, bit)
    OUTPUT_ROW(12, r18.b3, r19.b0, r19.b1, bit)
    #if OUTPUTS > 1
        OUTPUT_ROW(21, r19.b2, r19.b3, r20.b0, bit)
        OUTPUT_ROW(22, r20.b1, r20.b2, r20.b3, bit)
    #endif
    #if OUTPUTS > 2
        OUTPUT_ROW(31, r21.b0, r21.b1, r21.b2, bit)
        OUTPUT_ROW(32, r21.b3, r22.b0, r22.b1, bit)
    #endif
    #if OUTPUTS > 3
        OUTPUT_ROW(41, r22.b2, r22.b3, r23.b0, bit)
        OUTPUT_ROW(42, r23.b1, r23.b2, r23.b3, bit)
    #endif
    #if OUTPUTS > 4
        OUTPUT_ROW(51, r24.b0, r24.b1, r24.b2, bit)
        OUTPUT_ROW(52, r24.b3, r25.b0, r25.b1, bit)
    #endif
    #if OUTPUTS > 5
        OUTPUT_ROW(61, r25.b2, r25.b3, r26.b0, bit)
        OUTPUT_ROW(62, r26.b1, r26.b2, r26.b3, bit)
    #endif
    #if OUTPUTS > 6
        OUTPUT_ROW(71, r27.b0, r27.b1, r27.b2, bit)
        OUTPUT_ROW(72, r27.b3, r28.b0, r28.b1, bit)
    #endif
    #if OUTPUTS > 7
        OUTPUT_ROW(81, r28.b2, r28.b3, r29.b0, bit)
        OUTPUT_ROW(82, r29.b1, r29.b2, r29.b3, bit)
    #endif

    CLOCK_LO

    // All bits are configured;
    // the non-set ones will be cleared
    OUTPUT_GPIOS gpio0_set, gpio1_set, gpio2_set, gpio3_set

    CLOCK_HI
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

    // Wait for the start condition from the main program to indicate
    // that we have a rendered frame ready to clock out.  This also
    // handles the exit case if an invalid value is written to the start
    // start position.

    MOV gpio0_led_mask, 0
    MOV gpio1_led_mask, 0
    MOV gpio2_led_mask, 0
    MOV gpio3_led_mask, 0

    // Load the pointer to the buffer from PRU DRAM into r0.
    LBCO  data_addr, CONST_PRUDRAM, 0, 4


    CONFIGURE_OUTPUT(1)

#if OUTPUTS > 1
    CONFIGURE_PIN(r21)
    CONFIGURE_PIN(g21)
    CONFIGURE_PIN(b21)
    CONFIGURE_PIN(r22)
    CONFIGURE_PIN(g22)
    CONFIGURE_PIN(b22)
#endif
#if OUTPUTS > 2
    CONFIGURE_PIN(r31)
    CONFIGURE_PIN(g31)
    CONFIGURE_PIN(b31)
    CONFIGURE_PIN(r32)
    CONFIGURE_PIN(g32)
    CONFIGURE_PIN(b32)
#endif
#if OUTPUTS > 3
    CONFIGURE_PIN(r41)
    CONFIGURE_PIN(g41)
    CONFIGURE_PIN(b41)
    CONFIGURE_PIN(r42)
    CONFIGURE_PIN(g42)
    CONFIGURE_PIN(b42)
#endif
#if OUTPUTS > 4
    CONFIGURE_PIN(r51)
    CONFIGURE_PIN(g51)
    CONFIGURE_PIN(b51)
    CONFIGURE_PIN(r52)
    CONFIGURE_PIN(g52)
    CONFIGURE_PIN(b52)
#endif
#if OUTPUTS > 5
    CONFIGURE_PIN(r61)
    CONFIGURE_PIN(g61)
    CONFIGURE_PIN(b61)
    CONFIGURE_PIN(r62)
    CONFIGURE_PIN(g62)
    CONFIGURE_PIN(b62)
#endif
#if OUTPUTS > 6
    CONFIGURE_PIN(r71)
    CONFIGURE_PIN(g71)
    CONFIGURE_PIN(b71)
    CONFIGURE_PIN(r72)
    CONFIGURE_PIN(g72)
    CONFIGURE_PIN(b72)
#endif
#if OUTPUTS > 7
    CONFIGURE_PIN(r81)
    CONFIGURE_PIN(g81)
    CONFIGURE_PIN(b81)
    CONFIGURE_PIN(r82)
    CONFIGURE_PIN(g82)
    CONFIGURE_PIN(b82)
#endif


    ADJUST_SETTINGS

    DISABLE_PIN_INTERRUPTS

    LDI sleep_counter, 0
    LDI sleepDone, 0

    RESET_PRU_CLOCK gpio0_set, gpio1_set
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

NEW_ROW_LOOP:
    MOV bright, BITS

	ROW_LOOP:
		// Reset the latch pin; will be toggled at the end of the row
		LATCH_LO

        MOV pixelCount, ROW_LEN
        NEXT_PIXEL:

			// Load the sixteen RGB outputs into
			// consecutive registers, starting at pixel_data
            // we do this by transfering the data_addr to the other PRU via XOUT
            // which will LBBO the data and then transfer it back via XIN
            REREAD:
               XIN 11, gpio_base, 3*2*OUTPUTS + 4
               QBNE REREAD, gpio_base, data_addr
            ADD data_addr, data_addr, 3*2*OUTPUTS
            // XOUT the new data_addr so the other RPU can start working
            // on loading it while we process this data
            XOUT 10, data_addr, 4

            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 0
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 1
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 2
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 3
            CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 4
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 5
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 6
            //CHECK_FOR_DISPLAY_OFF
            OUTPUT_ROWS_FOR_BIT 7

            SUB pixelCount, pixelCount, 1;
            QBEQ DONE_PIXELS, pixelCount, 0;
            JMP NEXT_PIXEL
        DONE_PIXELS:

#ifdef ENABLESTATS
        //write some debug data into sram to read in c code
        GET_PRU_CLOCK gpio0_set, gpio2_set, 8
        MOV gpio2_set, sleep_counter
        QBNE STILLON, sleep_counter, 0
            MOV gpio2_set, sleepDone
        STILLON:
        MOV gpio3_set, statOffset
        LSL gpio3_set, gpio3_set, 2
        ADD gpio3_set, gpio3_set, 12
        SBCO gpio0_set, CONST_PRUDRAM, gpio3_set, 12
        ADD statOffset, statOffset, 3
#endif

        QBEQ DISPLAY_ALREADY_OFF, sleep_counter, 0
        WAIT_FOR_TIMER:
            GET_PRU_CLOCK gpio0_set, gpio1_set
            QBGT WAIT_FOR_TIMER, gpio0_set, sleep_counter
            DISPLAY_OFF
        DISPLAY_ALREADY_OFF:

        // determine on time (gpio0_set) and delay time (gpio1_set)
        GET_ON_DELAY_TIMES bright

        //QBNE NO_SET_ROW, bright, BITS //maxBitsToOutput
            OUTPUT_ROW_ADDRESS
        //NO_SET_ROW:

	    // Full data has been clocked out; latch it
	    LATCH_HI


        RESET_PRU_CLOCK gpio2_set, gpio3_set
        QBEQ NO_EXTRA_DELAY, gpio1_set, 0
            MOV sleep_counter, gpio1_set
            WAIT_FOR_EXTRA_OFF_TIME:
                GET_PRU_CLOCK gpio1_set, gpio2_set
                QBGT WAIT_FOR_EXTRA_OFF_TIME, gpio1_set, sleep_counter
        NO_EXTRA_DELAY:
        MOV sleep_counter, gpio0_set
        MOV sleepDone, 0

        DISPLAY_ON

		// Update the brightness, and then give the row another scan
		SUB bright, bright, 1

		QBGE ROW_DONE, bright, 0
            JMP ROW_LOOP
        ROW_DONE:

		// We have just done all eight brightness levels for this
		// row.  Time to move to the new row
        ADD row, row, 1

        QBNE READ_LOOP_DONE, row, ROWS
            JMP READ_LOOP
        READ_LOOP_DONE:

		JMP NEW_ROW_LOOP
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
