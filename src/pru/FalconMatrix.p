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
#ifndef  MATRIX_HEIGHT
#define MATRIX_HEIGHT 8		// 32x16 matrices
#endif

#ifndef OUTPUTS
#define OUTPUTS 8
#endif


#define RECORD_STATS

#define MAX_SLICE_LENGTH 0x8000

#define r11_gpio 2
#define r11_pin 2
#define g11_gpio 2
#define g11_pin 3
#define b11_gpio 2
#define b11_pin 5

#define r12_gpio 0
#define r12_pin 23
#define g12_gpio 2
#define g12_pin 4
#define b12_gpio 0
#define b12_pin 26

#define r21_gpio 0
#define r21_pin 27
#define g21_gpio 2
#define g21_pin 1
#define b21_gpio 0
#define b21_pin 22

#define r22_gpio 2
#define r22_pin 22
#define g22_gpio 2
#define g22_pin 23
#define b22_gpio 2
#define b22_pin 24

#define r31_gpio 0
#define r31_pin 30
#define g31_gpio 1
#define g31_pin 18
#define b31_gpio 0
#define b31_pin 31

#define r32_gpio 1
#define r32_pin 16
#define g32_gpio 0
#define g32_pin 3
#define b32_gpio 0
#define b32_pin 5

#define r41_gpio 0
#define r41_pin 2
#define g41_gpio 0
#define g41_pin 15
#define b41_gpio 1
#define b41_pin 17

#if 0
#define r42_gpio 1 // if we want to use PRU r30 output on clock
#define r42_pin 19
#else
#define r42_gpio 3 // if we use the boards as built
#define r42_pin 21
#endif
#define g42_gpio 3
#define g42_pin 19
#define b42_gpio 0
#define b42_pin 4

#define r51_gpio 2
#define r51_pin 25
#define g51_gpio 0
#define g51_pin 11
#define b51_gpio 0
#define b51_pin 10

#define r52_gpio 0
#define r52_pin 9
#define g52_gpio 0
#define g52_pin 8
#define b52_gpio 2
#define b52_pin 17

#define r61_gpio 2
#define r61_pin 16
#define g61_gpio 2
#define g61_pin 15
#define b61_gpio 2
#define b61_pin 14

#define r62_gpio 2
#define r62_pin 13
#define g62_gpio 2
#define g62_pin 10
#define b62_gpio 2
#define b62_pin 12

#define r71_gpio 2
#define r71_pin 11
#define g71_gpio 2
#define g71_pin 9
#define b71_gpio 2
#define b71_pin 8

#define r72_gpio 2
#define r72_pin 6
#define g72_gpio 3
#define g72_pin 18
#define b72_gpio 2
#define b72_pin 7

#define r81_gpio 3
#define r81_pin 17
#define g81_gpio 3
#define g81_pin 16
#define b81_gpio 3
#define b81_pin 15

#define r82_gpio 3
#define r82_pin 14
#define g82_gpio 0
#define g82_pin 14
#define b82_gpio 3
#define b82_pin 20

#define CAT3(X,Y,Z) X##Y##Z

// Control pins are all in GPIO1
#define gpio1_sel0 12 /* must be sequential with sel1 and sel2 */
#define gpio1_sel1 13
#define gpio1_sel2 14
#define gpio1_sel3 15
#define gpio1_latch 28
#define gpio1_oe 29
#define gpio1_clock 19

/** Generate a bitmask of which pins in GPIO0-3 are used.
 * 
 * \todo wtf "parameter too long": only 128 chars allowed?
 */

#define GPIO1_SEL_MASK (0\
|(1<<gpio1_sel0)\
|(1<<gpio1_sel1)\
|(1<<gpio1_sel2)\
|(1<<gpio1_sel3)\
)

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
#define data_addr r0
#define width r1
#define row r2.b1
#define bright r2.b2
#define sleep_counter r3
#define statOffset    r4
#define offset r5
#define out_clr r6 // must be one less than out_set
#define out_set r7
#define gpio0_set r7 // overloaded with out_set
#define gpio1_set r8
#define gpio2_set r9
#define gpio3_set r10
#define gpio0_led_mask r11
#define gpio1_led_mask r12
#define gpio2_led_mask r13
#define gpio3_led_mask r14
#define gpio_base r16
#define gpio1_base r17    // keep gpio1_base handy as it's used for clocks
#define pixel_data r18 // the next 12 registers, too;

#define CLOCK_HI \
	MOV out_clr, 1 << gpio1_clock; \
	SBBO out_clr, gpio1_base, GPIO_SETDATAOUT, 4; \
	//CLR r30,7

#define CLOCK_LO \
	MOV out_clr, 1 << gpio1_clock; \
	SBBO out_clr, gpio1_base, GPIO_CLRDATAOUT, 4; \
	//SET r30,7

#define LATCH_HI \
	MOV out_set, 1 << gpio1_latch; \
        SBBO out_set, gpio1_base, GPIO_SETDATAOUT, 4; \

#define LATCH_LO \
	MOV out_clr, 1 << gpio1_latch; \
        SBBO out_clr, gpio1_base, GPIO_CLRDATAOUT, 4; \

#define DISPLAY_OFF \
	MOV out_set, 1 << gpio1_oe; \
	SBBO out_set, gpio1_base, GPIO_SETDATAOUT, 4; \

#define DISPLAY_ON \
	MOV out_set, 1 << gpio1_oe; \
	SBBO out_set, gpio1_base, GPIO_CLRDATAOUT, 4; \

#define LATCH_AND_DISPLAY_ON \
    MOV out_set, 1 << gpio1_latch; \
    MOV out_clr, 1 << gpio1_oe; \
    SBBO out_clr, gpio1_base, GPIO_CLRDATAOUT, 8; \


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
    MOV		r1, CTPPR_0
    ST32	r0, r1

    // Configure the programmable pointer register for PRU0 by setting
    // c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    // 0x80001000 (DDR memory).
    MOV		r0, 0x00100000
    MOV		r1, CTPPR_1
    ST32	r0, r1

    // Write a 0x1 into the response field so that they know we have started
    MOV r2, #0x1
    SBCO r2, CONST_PRUDRAM, 12, 4

    // Wait for the start condition from the main program to indicate
    // that we have a rendered frame ready to clock out.  This also
    // handles the exit case if an invalid value is written to the start
    // start position.

        MOV gpio1_base, GPIO1
        MOV gpio0_led_mask, 0
        MOV gpio1_led_mask, 0
        MOV gpio2_led_mask, 0
        MOV gpio3_led_mask, 0

        // Load the pointer to the buffer from PRU DRAM into r0 and the
        // length (in pixels) into r1.
        LBCO      data_addr, CONST_PRUDRAM, 0, 28

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


    //we're going to clear the clock along with the pixel data
    SET  gpio1_led_mask, gpio1_led_mask, gpio1_clock

    LDI sleep_counter, 0

#ifdef RECORD_STATS
    RESET_PRU0_CLOCK gpio0_set, gpio1_set
    LDI statOffset, 0
#endif

READ_LOOP:
        // Load the pointer to the buffer from PRU DRAM into r0 and the
        // length (in pixels) into r1.
        LBCO      data_addr, CONST_PRUDRAM, 0, 28

        // Wait for a non-zero command
        QBEQ READ_LOOP, data_addr, #0

        // Command of 0xFF is the signal to exit
        QBEQ EXIT, data_addr, #0xFF

        MOV row, 0
        LDI statOffset, 0

NEW_ROW_LOOP:
		// Disable output while we set the address
		DISPLAY_OFF

		// set address; select pins in gpio1 are sequential
		// xor with the select bit mask to set which ones should
		LSL out_set, row, gpio1_sel0
		MOV out_clr, GPIO1_SEL_MASK
		AND out_set, out_set, out_clr // ensure no extra bits
		XOR out_clr, out_clr, out_set // complement the bits into clr
		SBBO out_clr, gpio1_base, GPIO_CLRDATAOUT, 8 // set both

		MOV bright, 7
	ROW_LOOP:
		// Re-start reading at the same row
		MOV offset, 0

		// Reset the latch pin; will be toggled at the end of the row
		LATCH_LO

		// compute where we are in the image
		PIXEL_LOOP:
			// Load the sixteen RGB outputs into
			// consecutive registers, starting at pixel_data.
			// This takes about 250 ns
			LBBO pixel_data, data_addr, offset, 3*2*OUTPUTS

            ZERO &gpio0_set, 16

            #define GPIO(R) CAT3(gpio,R,_set)
            #define OUTPUT_ROW(N,reg_r,reg_g,reg_b) \
                QBBC skip_r##N, reg_r, bright; \
                SET GPIO(r##N##_gpio), r##N##_pin; \
                skip_r##N: \
                QBBC skip_g##N, reg_g, bright; \
                SET GPIO(g##N##_gpio), g##N##_pin; \
                skip_g##N: \
                QBBC skip_b##N, reg_b, bright; \
                SET GPIO(b##N##_gpio), b##N##_pin; \
                skip_b##N: \

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
			// We write 8 bytes since CLR and DATA are contiguous,
			// which will write both the 0 and 1 bits in the
			// same instruction.  gpio0 and out_set are the same
			// register, so they must be done first.
			AND out_set, gpio0_set, gpio0_led_mask
			XOR out_clr, out_set, gpio0_led_mask
            MOV gpio_base, GPIO0
			SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 8

			AND out_set, gpio1_set, gpio1_led_mask
			XOR out_clr, out_set, gpio1_led_mask
			SBBO out_clr, gpio1_base, GPIO_CLRDATAOUT, 8

			AND out_set, gpio2_set, gpio2_led_mask
			XOR out_clr, out_set, gpio2_led_mask
            MOV gpio_base, GPIO2
			SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 8

            #if OUTPUTS > 3
                // Don't need gpio3 for outputs 1-3
                AND out_set, gpio3_set, gpio3_led_mask
                XOR out_clr, out_set, gpio3_led_mask
                MOV gpio_base, GPIO3
                SBBO out_clr, gpio_base, GPIO_CLRDATAOUT, 8
            #endif

			CLOCK_HI


            QBEQ NO_BLANK, sleep_counter, 0
                GET_PRU0_CLOCK gpio0_set, gpio1_set
                QBGT NO_BLANK, gpio0_set, sleep_counter
                DISPLAY_OFF
                LDI sleep_counter, 0
            NO_BLANK:

			ADD offset, offset, 3*16
			QBNE PIXEL_LOOP, offset, width


#ifdef RECORD_STATS
        //write some debug data into sram to read in c code
        GET_PRU0_CLOCK gpio0_set, gpio1_set
        MOV gpio1_set, sleep_counter
        MOV gpio3_set, statOffset
        LSL gpio3_set, gpio3_set, 2
        ADD gpio3_set, gpio3_set, 48
        SBCO gpio0_set, C24, gpio3_set, 8
        ADD statOffset, statOffset, 2
#endif
        WAIT_FOR_TIMER:
            GET_PRU0_CLOCK gpio0_set, gpio1_set
            QBGT WAIT_FOR_TIMER, gpio0_set, sleep_counter

        DISPLAY_OFF


        RESET_PRU0_CLOCK gpio0_set, gpio1_set
        LDI sleep_counter, MAX_SLICE_LENGTH
        LDI gpio0_set, 7;
        SUB gpio0_set, gpio0_set, bright
        LSR sleep_counter, sleep_counter, gpio0_set

		// Full data has been clocked out; latch it
		LATCH_AND_DISPLAY_ON

		// Update the brightness, and then give the row another scan
		SUB bright, bright, 1
		QBLT ROW_LOOP, bright, 0

		// We have just done all eight brightness levels for this
		// row.  Time to move to the new row

		// Increment our data_offset to point to the next row
		ADD data_addr, data_addr, offset

        ADD row, row, 1
        QBEQ READ_LOOP, row, MATRIX_HEIGHT

		QBA NEW_ROW_LOOP
EXIT:
#ifdef AM33XX
    // Send notification to Host for program completion
    MOV R31.b0, PRU0_ARM_INTERRUPT+16
#else
    MOV R31.b0, PRU0_ARM_INTERRUPT
#endif

    HALT
