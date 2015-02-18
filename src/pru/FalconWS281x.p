// \file
 /* WS281x LED strip driver for the BeagleBone Black.
 *
 * Drives up to 32 strips using the PRU hardware.  The ARM writes
 * rendered frames into shared DDR memory and sets a flag to indicate
 * how many pixels wide the image is.  The PRU then bit bangs the signal
 * out the 32 GPIO pins and sets a done flag.
 *
 * To stop, the ARM can write a 0xFF to the command, which will
 * cause the PRU code to exit.
 *
 * At 800 KHz:
 *  0 is 0.25 usec high, 1 usec low
 *  1 is 0.60 usec high, 0.65 usec low
 *  Reset is 50 usec
 *
 *  Pins are not contiguous.
 *  18 pins on GPIO0:   2  3  4  5  7  8  9 10 11 14 15 20 22 23 26 27 30 31
 *   3 pins on GPIO01: 16 17 18
 *  21 pins on GPIO02:  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 22 23 24 25
 *   6 pins on GPIO03: 14 15 16 17 19 21
 *  each pixel is stored in 4 bytes in the order GRBA (4th byte is ignored)
 * 
 */
 
// while len > 0:
// for bit# = 24 down to 0:
// delay 600 ns
// read 16 registers of data, build zero map for gpio0
// read 10 registers of data, build zero map for gpio1
// read  5 registers of data, build zero map for gpio3
//
// Send start pulse on all pins on gpio0, gpio1 and gpio3
// delay 250 ns
// bring zero pins low
// delay 300 ns
// bring all pins low
// increment address by 32

 //*
 //* So to clock this out:
 //*  ____
 //* |  | |______|
 //* 0  250 600  1250 offset
 //*    250 350   650 delta
 //* 
 //*/

#define r11_gpio        0
#define r11_pin		27
#define g11_gpio	2
#define g11_pin		1
#define b11_gpio	1
#define b11_pin		14

#define r12_gpio	1
#define r12_pin		15
#define g12_gpio	0
#define g12_pin		23
#define b12_gpio	0
#define b12_pin		26

#define r21_gpio	1
#define r21_pin		12
#define g21_gpio	1
#define g21_pin		13
#define b21_gpio	2
#define b21_pin		5

#define r22_gpio	2
#define r22_pin		4
#define g22_gpio	2
#define g22_pin		3
#define b22_gpio	2
#define b22_pin		2

#define r31_gpio	3
#define r31_pin		16
#define g31_gpio	0
#define g31_pin		20
#define b31_gpio	0
#define b31_pin		14

#define r32_gpio	3
#define r32_pin		19
#define g32_gpio	0
#define g32_pin		15
#define b32_gpio	3
#define b32_pin		21

#define r41_gpio	0
#define r41_pin		2
#define g41_gpio	1
#define g41_pin		17
#define b41_gpio	2
#define b41_pin		22

#define r42_gpio	0
#define r42_pin		3
#define g42_gpio	0
#define g42_pin		4
#define b42_gpio	2
#define b42_pin		24

#define r51_gpio	1
#define r51_pin		19
#define g51_gpio	0
#define g51_pin		5
#define b51_gpio	1
#define b51_pin		18

#define r52_gpio	1
#define r52_pin		16
#define g52_gpio	1
#define g52_pin		28
#define b52_gpio	0
#define b52_pin		31

#define r61_gpio	1
#define r61_pin		29
#define g61_gpio	0
#define g61_pin		30

// Note: From here down, these are garbage, we only have 32 outputs.
#define b61_gpio	0
#define b61_pin		1

#define r62_gpio	0
#define r62_pin		1
#define g62_gpio	0
#define g62_pin		1
#define b62_gpio	0
#define b62_pin		1

#define r71_gpio	0
#define r71_pin		1
#define g71_gpio	0
#define g71_pin		1
#define b71_gpio	0
#define b71_pin		1

#define r72_gpio	0
#define r72_pin		1
#define g72_gpio	0
#define g72_pin		1
#define b72_gpio	0
#define b72_pin		1

#define r81_gpio	0
#define r81_pin		1
#define g81_gpio	0
#define g81_pin		1
#define b81_gpio	0
#define b81_pin		1

#define r82_gpio	0
#define r82_pin		1
#define g82_gpio	0
#define g82_pin		1
#define b82_gpio	0
#define b82_pin		1


.origin 0
.entrypoint START

#include "FalconWS281x.hp"

/** Mappings of the GPIO devices */
#define GPIO0		0x44E07000
#define GPIO1		0x4804c000
#define GPIO2		0x481AC000
#define GPIO3		0x481AE000

/** Offsets for the clear and set registers in the devices */
#define GPIO_CLEARDATAOUT	0x190
#define GPIO_SETDATAOUT		0x194

/** Register map */
#define data_addr	r0
#define data_len	r1
#define gpio0_zeros	r2
#define gpio1_zeros	r3
#define gpio2_zeros	r4
#define gpio3_zeros	r5
#define bit_num		r6
#define sleep_counter	r7
// r10 - r22 are used for temp storage and bitmap processing
#define gpio0_led_mask	r26
#define gpio1_led_mask	r27
#define gpio2_led_mask	r28
#define gpio3_led_mask	r29


/** Sleep a given number of nanoseconds with 10 ns resolution.
 *
 * This busy waits for a given number of cycles.  Not for use
 * with things that must happen on a tight schedule.
 */
.macro SLEEPNS
.mparam ns,inst,lab
#ifdef CONFIG_WS2812
	MOV sleep_counter, (ns/5)-1-inst // ws2812 -- low speed
#else
	MOV sleep_counter, (ns/10)-1-inst // ws2811 -- high speed
#endif
lab:
	SUB sleep_counter, sleep_counter, 1
	QBNE lab, sleep_counter, 0
.endm


/** Wait for the cycle counter to reach a given value */
.macro WAITNS
.mparam ns,lab
	MOV r8, 0x22000 // control register
lab:
	LBBO r9, r8, 0xC, 4 // read the cycle counter
	SUB r9, r9, sleep_counter 
#ifdef CONFIG_WS2812
	QBGT lab, r9, 2*(ns)/5
#else
	QBGT lab, r9, (ns)/5
#endif
.endm

///** Macro to generate the mask of which bits are zero.
// * For each of these registers, set the
// * corresponding bit in the gpio0_zeros register if
// * the current bit is set in the strided register.
// */
//#define TEST_BIT(regN,gpioN,bitN) \
//	QBBS	gpioN##_##regN##_skip, regN, bit_num; \
//	SET	gpioN##_zeros, gpioN##_zeros, gpioN##_##bitN ; \
//	gpioN##_##regN##_skip: ; \

#define CAT3(X,Y,Z) X##Y##Z

#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)

#define GPIO(R)	CAT3(gpio,R,_zeros)

// Output the current bit for three LED strip outputs
// Note that this nomenclature was ported from the matrix code, and is
// mostly nonsense when applied to the strip code.
// Parameters:
// N: Output group to consider (11, 12, 21, ... 82)
// reg_r: register byte to read first strip data from  (ex: r10.b0)
// reg_g: register byte to read second strip data from (ex: r10.b1)
// reg_b: register byte to read third strip data from  (ex: r10.b2)
//
// Parameters from the environment:
// bit_num: current bit we're reading from
#define OUTPUT_ROW(N,reg_r,reg_g,reg_b)		\
	QBBS	skip_r##N, reg_r, bit_num;	\
	SET	GPIO(r##N##_gpio), r##N##_pin;	\
	skip_r##N:				\
	QBBS	skip_g##N, reg_g, bit_num;	\
	SET	GPIO(g##N##_gpio), g##N##_pin;	\
	skip_g##N:				\
	QBBS	skip_b##N, reg_b, bit_num;	\
	SET	GPIO(b##N##_gpio), b##N##_pin;	\
	skip_b##N:				\

START:
	// Enable OCP master port
	// clear the STANDBY_INIT bit in the SYSCFG register,
	// otherwise the PRU will not be able to write outside the
	// PRU memory space and to the BeagleBon's pins.
	LBCO	r0, C4, 4, 4
	CLR	r0, r0, 4
	SBCO	r0, C4, 4, 4

	// Configure the programmable pointer register for PRU0 by setting
	// c28_pointer[15:0] field to 0x0120.  This will make C28 point to
	// 0x00012000 (PRU shared RAM).
	MOV	r0, 0x00000120
	MOV	r1, CTPPR_0
	ST32	r0, r1

	// Configure the programmable pointer register for PRU0 by setting
	// c31_pointer[15:0] field to 0x0010.  This will make C31 point to
	// 0x80001000 (DDR memory).
	MOV	r0, 0x00100000
	MOV	r1, CTPPR_1
	ST32	r0, r1

	// Write a 0x1 into the response field so that they know we have started
	MOV	r2, #0x1
	SBCO	r2, CONST_PRUDRAM, 12, 4

	SET	GPIO_MASK(r11_gpio), r11_pin
	SET	GPIO_MASK(g11_gpio), g11_pin
	SET	GPIO_MASK(b11_gpio), b11_pin
	SET	GPIO_MASK(r12_gpio), r12_pin
	SET	GPIO_MASK(g12_gpio), g12_pin
	SET	GPIO_MASK(b12_gpio), b12_pin

	SET	GPIO_MASK(r21_gpio), r21_pin
	SET	GPIO_MASK(g21_gpio), g21_pin
	SET	GPIO_MASK(b21_gpio), b21_pin
	SET	GPIO_MASK(r22_gpio), r22_pin
	SET	GPIO_MASK(g22_gpio), g22_pin
	SET	GPIO_MASK(b22_gpio), b22_pin

	SET	GPIO_MASK(r31_gpio), r31_pin
	SET	GPIO_MASK(g31_gpio), g31_pin
	SET	GPIO_MASK(b31_gpio), b31_pin
	SET	GPIO_MASK(r32_gpio), r32_pin
	SET	GPIO_MASK(g32_gpio), g32_pin
	SET	GPIO_MASK(b32_gpio), b32_pin

	SET	GPIO_MASK(r41_gpio), r41_pin
	SET	GPIO_MASK(g41_gpio), g41_pin
	SET	GPIO_MASK(b41_gpio), b41_pin
	SET	GPIO_MASK(r42_gpio), r42_pin
	SET	GPIO_MASK(g42_gpio), g42_pin
	SET	GPIO_MASK(b42_gpio), b42_pin

	SET	GPIO_MASK(r51_gpio), r51_pin
	SET	GPIO_MASK(g51_gpio), g51_pin
	SET	GPIO_MASK(b51_gpio), b51_pin
	SET	GPIO_MASK(r52_gpio), r52_pin
	SET	GPIO_MASK(g52_gpio), g52_pin
	SET	GPIO_MASK(b52_gpio), b52_pin

	SET	GPIO_MASK(r61_gpio), r61_pin
	SET	GPIO_MASK(g61_gpio), g61_pin
	SET	GPIO_MASK(b61_gpio), b61_pin
	SET	GPIO_MASK(r62_gpio), r62_pin
	SET	GPIO_MASK(g62_gpio), g62_pin
	SET	GPIO_MASK(b62_gpio), b62_pin

	SET	GPIO_MASK(r71_gpio), r71_pin
	SET	GPIO_MASK(g71_gpio), g71_pin
	SET	GPIO_MASK(b71_gpio), b71_pin
	SET	GPIO_MASK(r72_gpio), r72_pin
	SET	GPIO_MASK(g72_gpio), g72_pin
	SET	GPIO_MASK(b72_gpio), b72_pin

	SET	GPIO_MASK(r81_gpio), r81_pin
	SET	GPIO_MASK(g81_gpio), g81_pin
	SET	GPIO_MASK(b81_gpio), b81_pin
	SET	GPIO_MASK(r82_gpio), r82_pin
	SET	GPIO_MASK(g82_gpio), g82_pin
	SET	GPIO_MASK(b82_gpio), b82_pin

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
	// Load the pointer to the buffer from PRU DRAM into r0 and the
	// length (in bytes-bit words) into r1.
	// start command into r2
	LBCO	data_addr, CONST_PRUDRAM, 0, 12

	// Wait for a non-zero command
	QBEQ	_LOOP, r2, #0

	// Zero out the start command so that they know we have received it
	// This allows maximum speed frame drawing since they know that they
	// can now swap the frame buffer pointer and write a new start command.
	MOV	r3, 0
	SBCO	r3, CONST_PRUDRAM, 8, 4

	// Command of 0xFF is the signal to exit
	QBEQ	EXIT, r2, #0xFF

	// The data len is in pixels; convert it to 3 channels * pixels
	ADD	r2, data_len, data_len
	ADD	data_len, data_len, r2

	WORD_LOOP:
		// for bit in 8 to 0; one color at a time
		MOV	bit_num, 8

		BIT_LOOP:
			SUB	bit_num, bit_num, 1
			// The idle period is 650 ns, but this is where
			// we do all of our work to read the RGB data and
			// repack it into bit slices.  Read the current counter
			// and then wait until 650 ns have passed once we complete
			// our work.
			// Disable the counter and clear it, then re-enable it
			MOV	r8, 0x22000 // control register
			LBBO	r9, r8, 0, 4
			CLR	r9, r9, 3 // disable counter bit
			SBBO	r9, r8, 0, 4 // write it back

			MOV	r10, 0
			SBBO	r10, r8, 0xC, 4 // clear the timer

			SET	r9, r9, 3 // enable counter bit
			SBBO	r9, r8, 0, 4 // write it back

			// Read the current counter value
			// Should be zero.
			LBBO	sleep_counter, r8, 0xC, 4


			// Load 48 bytes of data, starting at r10
			// one byte for each of the outputs
			LBBO	r10, r0, 0, 48
			MOV	gpio0_zeros, 0
			MOV	gpio1_zeros, 0
			MOV	gpio2_zeros, 0
			MOV	gpio3_zeros, 0

			OUTPUT_ROW(11, r10.b0, r10.b1, r10.b2)
			OUTPUT_ROW(12, r10.b3, r11.b0, r11.b1)
			OUTPUT_ROW(21, r11.b2, r11.b3, r12.b0)
			OUTPUT_ROW(22, r12.b1, r12.b2, r12.b3)

			OUTPUT_ROW(31, r13.b0, r13.b1, r13.b2)
			OUTPUT_ROW(32, r13.b3, r14.b0, r14.b1)
			OUTPUT_ROW(41, r14.b2, r14.b3, r15.b0)
			OUTPUT_ROW(42, r15.b1, r15.b2, r15.b3)

			OUTPUT_ROW(51, r16.b0, r16.b1, r16.b2)
			OUTPUT_ROW(52, r16.b3, r17.b0, r17.b1)
			OUTPUT_ROW(61, r17.b2, r17.b3, r18.b0)
			OUTPUT_ROW(62, r18.b1, r18.b2, r18.b3)

			OUTPUT_ROW(71, r19.b0, r19.b1, r19.b2)
			OUTPUT_ROW(72, r19.b3, r20.b0, r20.b1)
			OUTPUT_ROW(81, r20.b2, r20.b3, r21.b0)
			OUTPUT_ROW(82, r21.b1, r21.b2, r21.b3)

			// Now that we have read all of the data,
			// we can reuse the registers for the set/clear addresses
			// and the masks of which pins are mapped to which LEDs.
			MOV	r10, GPIO0 | GPIO_SETDATAOUT
			MOV	r11, GPIO1 | GPIO_SETDATAOUT
			MOV	r12, GPIO2 | GPIO_SETDATAOUT
			MOV	r13, GPIO3 | GPIO_SETDATAOUT

			// Wait for 650 ns to have passed
			WAITNS	650, wait_start_time

			// Send all the start bits
			SBBO	gpio0_led_mask, r10, 0, 4
			SBBO	gpio1_led_mask, r11, 0, 4
			SBBO	gpio2_led_mask, r12, 0, 4
			SBBO	gpio3_led_mask, r13, 0, 4

			// Reconfigure r10-13 for clearing the bits
			MOV	r10, GPIO0 | GPIO_CLEARDATAOUT
			MOV	r11, GPIO1 | GPIO_CLEARDATAOUT
			MOV	r12, GPIO2 | GPIO_CLEARDATAOUT
			MOV	r13, GPIO3 | GPIO_CLEARDATAOUT

			// wait for the length of the zero bits (250 ns)
			WAITNS	650+250, wait_zero_time
			//SLEEPNS 250, 1, wait_zero_time

			// turn off all the zero bits
			SBBO	gpio0_zeros, r10, 0, 4
			SBBO	gpio1_zeros, r11, 0, 4
			SBBO	gpio2_zeros, r12, 0, 4
			SBBO	gpio3_zeros, r13, 0, 4

			// Wait until the length of the one bits
			// TODO: Fix WAITNS so it can incorporate both delays?
			WAITNS	650+600, wait_one_time
			SLEEPNS	500, 1, sleep_one_time  // Wait a little longer to fix the timing

			// Turn all the bits off
			SBBO	gpio0_led_mask, r10, 0, 4
			SBBO	gpio1_led_mask, r11, 0, 4
			SBBO	gpio2_led_mask, r12, 0, 4
			SBBO	gpio3_led_mask, r13, 0, 4

			QBNE	BIT_LOOP, bit_num, 0

		// The 48 RGB streams have been clocked out
		// Move to the next color component for each pixel
		ADD	data_addr, data_addr, 48
		SUB	data_len, data_len, 1
		QBNE	WORD_LOOP, data_len, #0

	// Delay at least 50 usec; this is the required reset
	// time for the LED strip to update with the new pixels.
	SLEEPNS	50000, 1, reset_time

	// Write out that we are done!
	// Store a non-zero response in the buffer so that they know that we are done
	// aso a quick hack, we write the counter so that we know how
	// long it took to write out.
	MOV	r8, 0x22000 // control register
	LBBO	r2, r8, 0xC, 4
	SBCO	r2, CONST_PRUDRAM, 12, 4

	// Go back to waiting for the next frame buffer
	QBA	_LOOP

EXIT:
	// Write a 0xFF into the response field so that they know we're done
	MOV r2, #0xFF
	SBCO r2, CONST_PRUDRAM, 12, 4

#ifdef AM33XX
	// Send notification to Host for program completion
	MOV R31.b0, PRU0_ARM_INTERRUPT+16
#else
	MOV R31.b0, PRU0_ARM_INTERRUPT
#endif

	HALT
