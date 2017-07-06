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

// number of panels chained per output
// this needs to match the similar define found in the ledscape.h
#define LEDSCAPE_MATRIX_PANELS 12


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
#define row             r1.b0
#define bright          r1.b1
#define bit             r1.b2
#define bitFlags        r1.b3
#define statsBit        0
#define sleep_counter   r2
#define statOffset      r3.w0
#define numRows         r3.b2
//unused                r3.b3
#define offset r4
#define initialOffset r5
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

#ifndef OCTO_V2
#include "OctoscrollerV1.hp"
#else
#include "OctoscrollerV2.hp"
#endif

#ifdef gpio_clock
.macro CLOCK_HI
    MOV out_set, 1 << gpio_clock
    SBBO out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
.endm

.macro CLOCK_LO
    // we can lower the clock line at the same time as outputing the
    // gpio1 data so this doesn't need to be implemented

    //MOV out_clr, 1 << gpio_clock
    //SBBO out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
.endm
#endif

#ifdef gpio_latch
.macro LATCH_HI
    MOV out_set, 1 << gpio_latch
    SBBO out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
.endm

.macro LATCH_LO
    // we can lower the latch line at the same time as outputing the
    // gpio1 data so this doesn't need to be implemented
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

#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)
	SET GPIO_MASK(r11_gpio), r11_pin
	SET GPIO_MASK(g11_gpio), g11_pin
	SET GPIO_MASK(b11_gpio), b11_pin
	SET GPIO_MASK(r12_gpio), r12_pin
	SET GPIO_MASK(g12_gpio), g12_pin
	SET GPIO_MASK(b12_gpio), b12_pin

#if OUTPUTS > 1
    SET GPIO_MASK(r21_gpio), r21_pin
    SET GPIO_MASK(g21_gpio), g21_pin
    SET GPIO_MASK(b21_gpio), b21_pin
    SET GPIO_MASK(r22_gpio), r22_pin
    SET GPIO_MASK(g22_gpio), g22_pin
    SET GPIO_MASK(b22_gpio), b22_pin
#endif
#if OUTPUTS > 2
    SET GPIO_MASK(r31_gpio), r31_pin
    SET GPIO_MASK(g31_gpio), g31_pin
    SET GPIO_MASK(b31_gpio), b31_pin
    SET GPIO_MASK(r32_gpio), r32_pin
    SET GPIO_MASK(g32_gpio), g32_pin
    SET GPIO_MASK(b32_gpio), b32_pin
#endif
#if OUTPUTS > 3
    SET GPIO_MASK(r41_gpio), r41_pin
    SET GPIO_MASK(g41_gpio), g41_pin
    SET GPIO_MASK(b41_gpio), b41_pin
    SET GPIO_MASK(r42_gpio), r42_pin
    SET GPIO_MASK(g42_gpio), g42_pin
    SET GPIO_MASK(b42_gpio), b42_pin
#endif
#if OUTPUTS > 4
    SET GPIO_MASK(r51_gpio), r51_pin
    SET GPIO_MASK(g51_gpio), g51_pin
    SET GPIO_MASK(b51_gpio), b51_pin
    SET GPIO_MASK(r52_gpio), r52_pin
    SET GPIO_MASK(g52_gpio), g52_pin
    SET GPIO_MASK(b52_gpio), b52_pin
#endif
#if OUTPUTS > 5
    SET GPIO_MASK(r61_gpio), r61_pin
    SET GPIO_MASK(g61_gpio), g61_pin
    SET GPIO_MASK(b61_gpio), b61_pin
    SET GPIO_MASK(r62_gpio), r62_pin
    SET GPIO_MASK(g62_gpio), g62_pin
    SET GPIO_MASK(b62_gpio), b62_pin
#endif
#if OUTPUTS > 6
    SET GPIO_MASK(r71_gpio), r71_pin
    SET GPIO_MASK(g71_gpio), g71_pin
    SET GPIO_MASK(b71_gpio), b71_pin
    SET GPIO_MASK(r72_gpio), r72_pin
    SET GPIO_MASK(g72_gpio), g72_pin
    SET GPIO_MASK(b72_gpio), b72_pin
#endif
#if OUTPUTS > 7
    SET GPIO_MASK(r81_gpio), r81_pin
    SET GPIO_MASK(g81_gpio), g81_pin
    SET GPIO_MASK(b81_gpio), b81_pin
    SET GPIO_MASK(r82_gpio), r82_pin
    SET GPIO_MASK(g82_gpio), g82_pin
    SET GPIO_MASK(b82_gpio), b82_pin
#endif


    ADJUST_SETTINGS

    LDI sleep_counter, 0

    RESET_PRU_CLOCK gpio0_set, gpio1_set
    LDI statOffset, 0

READ_LOOP:
        // Load the pointer to the buffer from PRU DRAM into r0 and the
        // length (in pixels) into r1.
        LBCO data_addr, CONST_PRUDRAM, 0, 4

        // Wait for a non-zero command
        QBEQ READ_LOOP, data_addr, #0

        // Command of 0xFF is the signal to exit
        QBEQ EXIT, data_addr, #0xFF

        LDI bitFlags, 0

        //load the configuration, temporarily use the _set registers
        LBCO      gpio0_set, CONST_PRUDRAM, 16, 8
        QBEQ      NO_STATS_FLAG, gpio1_set, 0
            SET      bitFlags, statsBit
        NO_STATS_FLAG:

        MOV numRows, gpio0_set.w2

        LDI initialOffset, LEDSCAPE_MATRIX_PANELS
        SUB gpio0_set, initialOffset, gpio0_set.w0
        LDI initialOffset, 0

        QBEQ NO_OFFSET, gpio0_set, 0
        LDI gpio1_set, 3*2*8*4 //192 bytes per panel
        CALC_OFFSETT:
            ADD initialOffset, initialOffset, gpio1_set
            SUB gpio0_set, gpio0_set, 1
            QBNE CALC_OFFSETT, gpio0_set, 0
        NO_OFFSET:



        MOV row, 0
        LDI statOffset, 0
        LDI offset, 0

NEW_ROW_LOOP:
    MOV bright, 8

	ROW_LOOP:
		// Reset the latch pin; will be toggled at the end of the row
		LATCH_LO

		// compute where we are in the image
        LOOP DONE_PIXELS, (LEDSCAPE_MATRIX_PANELS * 4)
            QBLT SKIP_DATA, initialOffset, offset

			// Load the sixteen RGB outputs into
			// consecutive registers, starting at pixel_data.
            LBBO pixel_data, data_addr, offset, 3*2*OUTPUTS

            LDI bit, 0
            BIT_LOOP:
                CHECK_FOR_DISPLAY_OFF

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
            CHECK_FOR_DISPLAY_OFF

			ADD offset, offset, 3*16
        DONE_PIXELS:

        QBBC NO_STATS, bitFlags, statsBit
            //write some debug data into sram to read in c code
            GET_PRU_CLOCK gpio0_set, gpio2_set, 8
            MOV gpio2_set, sleep_counter
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

        LDI gpio0_set, 8
        SUB gpio0_set, gpio0_set, bright
        LSL gpio0_set, gpio0_set, 3
        ADD gpio0_set, gpio0_set, 24
        LBCO gpio0_set, CONST_PRUDRAM, gpio0_set, 8


        QBNE NO_SET_ROW, bright, 8
            OUTPUT_ROW_ADDRESS
        NO_SET_ROW:

		// Full data has been clocked out; latch it
		LATCH_HI

        RESET_PRU_CLOCK gpio2_set, gpio3_set
        MOV sleep_counter, gpio1_set
        WAIT_FOR_EXTRA_OFF_TIME:
            GET_PRU_CLOCK gpio1_set, gpio2_set
            QBGT WAIT_FOR_EXTRA_OFF_TIME, gpio1_set, sleep_counter
        MOV sleep_counter, gpio0_set

        DISPLAY_ON

		// Update the brightness, and then give the row another scan
		SUB bright, bright, 1
        // Increment our data_offset to point to the next row
        ADD data_addr, data_addr, offset
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
