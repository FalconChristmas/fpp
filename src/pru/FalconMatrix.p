/* PRU based 16x32 LED Matrix driver.
 *
 * Drives up to sixteen 16x32 matrices using the PRU hardware.
 *
 * Uses sixteen data pins in GPIO0 (one for each data line on each
 * matrix) and six control pins in GPIO1 shared between all the matrices.
 *
 * The ARM writes a 24-bit color 512x16 into the shared RAM, sets the
 * frame buffer pointer in the command structure and the PRU clocks it out
 * to the sixteen matrices.  Since there is no PWM in the matrix hardware,
 * the PRU will cycle through various brightness levels. After each PWM
 * cycle it rechecks the frame buffer pointer, allowing a glitch-free
 * transition to a new frame.
 *
 * To pause the redraw loop, write a NULL to the buffer pointer.
 * To shut down the PRU, write -1 to the buffer pointer.
 */
#ifndef OUTPUTS
#define OUTPUTS 8
#endif

#define RUNNING_ON_PRU1


#define CAT3(X,Y,Z) X##Y##Z



.origin 0
.entrypoint START

#include "FalconWS281x.hp"
#include "FalconUtils.hp"

/** Mappings of the GPIO devices */
#define GPIO0 (0x44E07000 + 0x100)
#define GPIO1 (0x4804c000 + 0x100)
#define GPIO2 (0x481AC000 + 0x100)
#define GPIO3 (0x481AE000 + 0x100)

/** Offsets for the clear and set registers in the devices.
 * Since the offsets can only be 0xFF, we deliberately add offsets
 */
#define GPIO_CLRDATAOUT (0x190 - 0x100)
#define GPIO_SETDATAOUT (0x194 - 0x100)

/** Register map */
#define data_addr       r0
#define initialOffset   r1.w0
#define pixelsPerRow    r1.w2
#define row             r2.b0
#define bright          r2.b1
#define bit             r2.b2
#define bitFlags        r2.b3
#define statsBit        0
#define sleep_counter   r3.w0
#define sleepDone       r3.w2
#define statOffset      r4.w0
#define numRows         r4.b2
#define bitsToSkip      r4.b3
#define offset r5
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
#define gpio_base r16
#define gpio_base_cache r17    // register to keep a base address cached.  handy if it's used for clocks
#define pixel_data r18 // the next 12 registers, too;



#define GPIO(R) CAT3(gpio,R,_set)
#define OUTPUT_ROW(N,reg_r,reg_g,reg_b) \
    QBBC skip_r##N, reg_r, bit; \
    SET GPIO(r##N##_gpio), r##N##_pin; \
    skip_r##N: \
    QBBC skip_g##N, reg_g, bit; \
    SET GPIO(g##N##_gpio), g##N##_pin; \
    skip_g##N: \
    QBBC skip_b##N, reg_b, bit; \
    SET GPIO(b##N##_gpio), b##N##_pin; \
    skip_b##N: \


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
#elif defined OCTO_V2
#include "OctoscrollerV2.hp"
#elif defined POCKETSCROLLER_V1
#include "PocketScrollerV1.hp"
#else
#include "OctoscrollerV1.hp"
#endif

#ifdef gpio_clock
.macro CLOCK_HI
    MOV out_set, 1 << gpio_clock
    SBBO out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
.endm

.macro CLOCK_LO
    // we normally can lower the clock line at the same time as outputing the
    // gpio data so this doesn't need to be implemented, however, the PocketBeagle
    // has issues as this occurs to quick so we need to do it independently

#ifdef NEED_CLOCK_LO
    MOV out_clr, 1 << gpio_clock
    SBBO out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
#endif
.endm
#endif

#ifdef gpio_latch
.macro LATCH_HI
    MOV out_set, 1 << gpio_latch
    SBBO out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
.endm

.macro LATCH_LO
    // we can lower the latch line at the same time as outputing the
    // gpio data so this doesn't need to be implemented
    //MOV out_clr, 1 << gpio_latch
    //SBBO out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
.endm
#endif

#ifdef gpio_oe
.macro DISPLAY_OFF
    MOV out_set, 1 << gpio_oe
    SBBO out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
.endm

.macro DISPLAY_ON
    MOV out_clr, 1 << gpio_oe
    SBBO out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
.endm
#endif



.macro OUTPUT_ROW_ADDRESS
    // set address; select pins in gpio1 are sequential
    // xor with the select bit mask to set which ones should
    LSL out_set, row, gpio_sel0
    MOV out_clr, GPIO_SEL_MASK
    AND out_set, out_set, out_clr // ensure no extra bits
    XOR out_clr, out_clr, out_set // complement the bits into clr
    SBBO out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 8 // set both
.endm


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
    SBCO r2, CONST_PRUDRAM, 12, 4

    // Wait for the start condition from the main program to indicate
    // that we have a rendered frame ready to clock out.  This also
    // handles the exit case if an invalid value is written to the start
    // start position.

    MOV gpio0_led_mask, 0
    MOV gpio1_led_mask, 0
    MOV gpio2_led_mask, 0
    MOV gpio3_led_mask, 0

    // Load the pointer to the buffer from PRU DRAM into r0.
    LBCO      data_addr, CONST_PRUDRAM, 0, 4


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
        // Load the pointer to the buffer from PRU DRAM into r0 and the
        // length (in pixels) into r1.
        LBCO data_addr, CONST_PRUDRAM, 0, 8

        // Wait for a non-zero command
        QBEQ READ_LOOP, data_addr, #0

        // Command of 0xFF is the signal to exit
        QBEQ EXIT, data_addr, #0xFF

        MOV pixelsPerRow, r1
        LDI bitFlags, 0

        //load the configuration, temporarily use the _set registers
        LBCO      gpio0_set, CONST_PRUDRAM, 16, 8
        QBEQ      NO_STATS_FLAG, gpio1_set, 0
            SET      bitFlags, statsBit
        NO_STATS_FLAG:

        MOV numRows, gpio0_set.b2
        LDI bitsToSkip, 8
        SUB bitsToSkip, bitsToSkip, gpio0_set.b3
        MOV initialOffset, gpio0_set.w0


        MOV row, 0
        LDI statOffset, 0
        LDI offset, 0

NEW_ROW_LOOP:
    MOV bright, 8

	ROW_LOOP:
		// Reset the latch pin; will be toggled at the end of the row
		LATCH_LO

		// compute where we are in the image
        LOOP DONE_PIXELS, pixelsPerRow
            CHECK_FOR_DISPLAY_OFF
            QBLT SKIP_DATA, initialOffset, offset

			// Load the sixteen RGB outputs into
			// consecutive registers, starting at pixel_data.
            LBBO pixel_data, data_addr, offset, 3*2*OUTPUTS

            CHECK_FOR_DISPLAY_OFF

            LDI bit, 0
            BIT_LOOP:
                ZERO &gpio0_set, 16

                CLOCK_LO

                OUTPUT_ROW(11, r18.b0, r18.b1, r18.b2)
                OUTPUT_ROW(12, r18.b3, r19.b0, r19.b1)
    #if OUTPUTS > 1
                OUTPUT_ROW(21, r19.b2, r19.b3, r20.b0)
                OUTPUT_ROW(22, r20.b1, r20.b2, r20.b3)
    #endif
    #if OUTPUTS > 2
                OUTPUT_ROW(31, r21.b0, r21.b1, r21.b2)
                OUTPUT_ROW(32, r21.b3, r22.b0, r22.b1)
    #endif
    #if OUTPUTS > 3
                OUTPUT_ROW(41, r22.b2, r22.b3, r23.b0)
                OUTPUT_ROW(42, r23.b1, r23.b2, r23.b3)
    #endif
    #if OUTPUTS > 4
                OUTPUT_ROW(51, r24.b0, r24.b1, r24.b2)
                OUTPUT_ROW(52, r24.b3, r25.b0, r25.b1)
    #endif
    #if OUTPUTS > 5
                OUTPUT_ROW(61, r25.b2, r25.b3, r26.b0)
                OUTPUT_ROW(62, r26.b1, r26.b2, r26.b3)
    #endif
    #if OUTPUTS > 6
                OUTPUT_ROW(71, r27.b0, r27.b1, r27.b2)
                OUTPUT_ROW(72, r27.b3, r28.b0, r28.b1)
    #endif
    #if OUTPUTS > 7
                OUTPUT_ROW(81, r28.b2, r28.b3, r29.b0)
                OUTPUT_ROW(82, r29.b1, r29.b2, r29.b3)
    #endif

                // All bits are configured;
                // the non-set ones will be cleared
                OUTPUT_GPIOS gpio0_set, gpio1_set, gpio2_set, gpio3_set

                CLOCK_HI

            ADD bit, bit, 1
            QBNE BIT_LOOP, bit, 8

            SKIP_DATA:
			ADD offset, offset, 3*16
        DONE_PIXELS:

        QBBC NO_STATS, bitFlags, statsBit
            //write some debug data into sram to read in c code
            GET_PRU_CLOCK gpio0_set, gpio2_set, 8
            MOV gpio2_set, sleep_counter
            QBNE STILLON, sleep_counter, 0
                MOV gpio2_set, sleepDone
            STILLON:
            MOV gpio3_set, statOffset
            LSL gpio3_set, gpio3_set, 2
            ADD gpio3_set, gpio3_set, 88   // move past all the config at the beginning
            SBCO gpio0_set, C24, gpio3_set, 12
            ADD statOffset, statOffset, 3
        NO_STATS:

        QBEQ DISPLAY_ALREADY_OFF, sleep_counter, 0
        WAIT_FOR_TIMER:
            GET_PRU_CLOCK gpio0_set, gpio1_set
            QBGT WAIT_FOR_TIMER, gpio0_set, sleep_counter
            DISPLAY_OFF
        DISPLAY_ALREADY_OFF:

        MOV gpio0_set, 8 //maxBitsToOutput
        SUB gpio0_set, gpio0_set, bright
        LSL gpio0_set, gpio0_set, 3
        ADD gpio0_set, gpio0_set, 24
        LBCO gpio0_set, CONST_PRUDRAM, gpio0_set, 8


        QBNE NO_SET_ROW, bright, 8 //maxBitsToOutput
            OUTPUT_ROW_ADDRESS
        NO_SET_ROW:

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
        // Increment our data_offset to point to the next row
        ADD data_addr, data_addr, offset

        QBNE NOSKIPROW, bitsToSkip, bright
        LOOP NOSKIPROW, bitsToSkip
            ADD data_addr, data_addr, offset
            LDI bright, 0
        NOSKIPROW:

        LDI offset, 0

		QBLT ROW_LOOP, bright, 0

		// We have just done all eight brightness levels for this
		// row.  Time to move to the new row
        ADD row, row, 1
        QBEQ READ_LOOP, row, numRows

		QBA NEW_ROW_LOOP
EXIT:
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU_ARM_INTERRUPT
#endif

    HALT
