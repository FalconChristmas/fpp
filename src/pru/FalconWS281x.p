// \file
 /* WS281x LED strip driver for the BeagleBone Black.
 *
 * Drives up to 48 strips using the PRU hardware.  The ARM writes
 * rendered frames into shared DDR memory and sets a flag to indicate
 * how many pixels wide the image is.  The PRU then bit bangs the signal
 * out the 48 GPIO pins and sets a done flag.
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
// read 12 registers of data, build zero map for gpio0
// read 10 registers of data, build zero map for gpio1
// read  5 registers of data, build zero map for gpio3
//
// Send start pulse on all pins on gpio0, gpio1 and gpio3
// delay 250 ns
// bring zero pins low
// delay 300 ns
// bring all pins low
// increment address by 48

 //*
 //* So to clock this out:
 //*  ____
 //* |  | |______|
 //* 0  250 600  1250 offset
 //*    250 350   650 delta
 //* 
 //*/


#if defined F4B
#include "F4B.hp"
#elif defined F8B
#include "F8B.hp"
#else
#include "F16B.hp"
#endif



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
//r8 and r9 are used in WAITNS, can be used for other outside of that
//r10 - r11 are used for data storage and bitmap processing

//tmp registers, mostly used for preparing output
#define tmp0            r22
#define tmp1            r23
#define tmp2            r24
#define tmp3            r25

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


// Output the current bit for a LED strip output
// Parameters:
// N: Output group to consider (11, 12, 21, ... 82)
//
// Parameters from the environment:
// bit_num: current bit we're reading from
#define OUTPUT_STRIP(N)		\
    QBBS	skip_o##N, o##N##_dreg, bit_num;	\
    SET     GPIO(o##N##_gpio), o##N##_pin;	\
    skip_o##N:

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


    SET	GPIO_MASK(o1_gpio), o1_pin
    SET	GPIO_MASK(o2_gpio), o2_pin
    SET	GPIO_MASK(o3_gpio), o3_pin
    SET	GPIO_MASK(o4_gpio), o4_pin
#if OUTPUTS > 4
    SET	GPIO_MASK(o5_gpio), o5_pin
    SET	GPIO_MASK(o6_gpio), o6_pin
    SET	GPIO_MASK(o7_gpio), o7_pin
    SET	GPIO_MASK(o8_gpio), o8_pin
    SET	GPIO_MASK(o9_gpio), o9_pin
    SET	GPIO_MASK(o10_gpio), o10_pin
    SET	GPIO_MASK(o11_gpio), o11_pin
    SET	GPIO_MASK(o12_gpio), o12_pin
#endif
#if OUTPUTS > 12
    SET	GPIO_MASK(o13_gpio), o13_pin
    SET	GPIO_MASK(o14_gpio), o14_pin
    SET	GPIO_MASK(o15_gpio), o15_pin
    SET	GPIO_MASK(o16_gpio), o16_pin
#endif
#if OUTPUTS > 16
    SET	GPIO_MASK(o17_gpio), o17_pin
    SET	GPIO_MASK(o18_gpio), o18_pin
    SET	GPIO_MASK(o19_gpio), o19_pin
    SET	GPIO_MASK(o20_gpio), o20_pin
#endif
#if OUTPUTS > 20
    SET	GPIO_MASK(o21_gpio), o21_pin
    SET	GPIO_MASK(o22_gpio), o22_pin
    SET	GPIO_MASK(o23_gpio), o23_pin
    SET	GPIO_MASK(o24_gpio), o24_pin
#endif
#if OUTPUTS > 24
    SET	GPIO_MASK(o25_gpio), o25_pin
    SET	GPIO_MASK(o26_gpio), o26_pin
    SET	GPIO_MASK(o27_gpio), o27_pin
    SET	GPIO_MASK(o28_gpio), o28_pin
    SET	GPIO_MASK(o29_gpio), o29_pin
    SET	GPIO_MASK(o30_gpio), o30_pin
    SET	GPIO_MASK(o31_gpio), o31_pin
    SET	GPIO_MASK(o32_gpio), o32_pin
    SET	GPIO_MASK(o33_gpio), o33_pin
    SET	GPIO_MASK(o34_gpio), o34_pin
    SET	GPIO_MASK(o35_gpio), o35_pin
    SET	GPIO_MASK(o36_gpio), o36_pin
    SET	GPIO_MASK(o37_gpio), o37_pin
    SET	GPIO_MASK(o38_gpio), o38_pin
    SET	GPIO_MASK(o39_gpio), o39_pin
    SET	GPIO_MASK(o40_gpio), o40_pin
#endif
#if OUTPUTS > 40
    SET	GPIO_MASK(o41_gpio), o41_pin
    SET	GPIO_MASK(o42_gpio), o42_pin
    SET	GPIO_MASK(o43_gpio), o43_pin
    SET	GPIO_MASK(o44_gpio), o44_pin
    SET	GPIO_MASK(o45_gpio), o45_pin
    SET	GPIO_MASK(o46_gpio), o46_pin
    SET	GPIO_MASK(o47_gpio), o47_pin
    SET	GPIO_MASK(o48_gpio), o48_pin
#endif

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

        // Load 48 bytes of data, starting at r10
        // one byte for each of the outputs
        LBBO	r10, r0, 0, OUTPUTS

		BIT_LOOP:
			SUB     bit_num, bit_num, 1
			// The idle period is 650 ns, but this is where
			// we do all of our work to read the RGB data and
			// repack it into bit slices.  Read the current counter
			// and then wait until 650 ns have passed once we complete
			// our work.
			// Disable the counter and clear it, then re-enable it
			MOV     r8, 0x22000 // control register
			LBBO	r9, r8, 0, 4
			CLR     r9, r9, 3 // disable counter bit
			SBBO	r9, r8, 0, 4 // write it back

			MOV     tmp0, 0
			SBBO	tmp0, r8, 0xC, 4 // clear the timer

			SET     r9, r9, 3 // enable counter bit
			SBBO	r9, r8, 0, 4 // write it back

			// Read the current counter value
			// Should be zero.
			LBBO	sleep_counter, r8, 0xC, 4

			MOV	gpio0_zeros, 0
			MOV	gpio1_zeros, 0
			MOV	gpio2_zeros, 0
			MOV	gpio3_zeros, 0

			OUTPUT_STRIP(1)
            OUTPUT_STRIP(2)
            OUTPUT_STRIP(3)
            OUTPUT_STRIP(4)
#if OUTPUTS > 4
            OUTPUT_STRIP(5)
            OUTPUT_STRIP(6)
            OUTPUT_STRIP(7)
            OUTPUT_STRIP(8)
            OUTPUT_STRIP(9)

            OUTPUT_STRIP(10)
            OUTPUT_STRIP(11)
            OUTPUT_STRIP(12)
#endif
#if OUTPUTS > 12
            OUTPUT_STRIP(13)
            OUTPUT_STRIP(14)
            OUTPUT_STRIP(15)
            OUTPUT_STRIP(16)
#endif
#if OUTPUTS > 16
            OUTPUT_STRIP(17)
            OUTPUT_STRIP(18)
            OUTPUT_STRIP(19)
            OUTPUT_STRIP(20)
#endif
#if OUTPUTS > 20
            OUTPUT_STRIP(21)
            OUTPUT_STRIP(22)
            OUTPUT_STRIP(23)
            OUTPUT_STRIP(24)
#endif
#if OUTPUTS > 24
            OUTPUT_STRIP(25)
            OUTPUT_STRIP(26)
            OUTPUT_STRIP(27)
            OUTPUT_STRIP(28)
            OUTPUT_STRIP(29)

            OUTPUT_STRIP(30)
            OUTPUT_STRIP(31)
            OUTPUT_STRIP(32)
            OUTPUT_STRIP(33)
            OUTPUT_STRIP(34)
            OUTPUT_STRIP(35)
            OUTPUT_STRIP(36)
            OUTPUT_STRIP(37)
            OUTPUT_STRIP(38)
            OUTPUT_STRIP(39)

            OUTPUT_STRIP(40)
#endif
#if OUTPUTS > 40
            OUTPUT_STRIP(41)
            OUTPUT_STRIP(42)
            OUTPUT_STRIP(43)
            OUTPUT_STRIP(44)
            OUTPUT_STRIP(45)
            OUTPUT_STRIP(46)
            OUTPUT_STRIP(47)
            OUTPUT_STRIP(48)
#endif

			MOV	tmp0, GPIO0 | GPIO_SETDATAOUT
#ifdef USES_GPIO1
			MOV	tmp1, GPIO1 | GPIO_SETDATAOUT
#endif
			MOV	tmp2, GPIO2 | GPIO_SETDATAOUT
#ifdef USES_GPIO3
			MOV	tmp3, GPIO3 | GPIO_SETDATAOUT
#endif

			// Wait for 650 ns to have passed
			WAITNS	650, wait_start_time

			// Send all the start bits
			SBBO	gpio0_led_mask, tmp0, 0, 4
#ifdef USES_GPIO1
			SBBO	gpio1_led_mask, tmp1, 0, 4
#endif
			SBBO	gpio2_led_mask, tmp2, 0, 4
#ifdef USES_GPIO3
			SBBO	gpio3_led_mask, tmp3, 0, 4
#endif

			// Reconfigure tmps for clearing the bits
			MOV	tmp0, GPIO0 | GPIO_CLEARDATAOUT
#ifdef USES_GPIO1
			MOV	tmp1, GPIO1 | GPIO_CLEARDATAOUT
#endif
			MOV	tmp2, GPIO2 | GPIO_CLEARDATAOUT
#ifdef USES_GPIO3
			MOV	tmp3, GPIO3 | GPIO_CLEARDATAOUT
#endif

			// wait for the length of the zero bits (250 ns)
			WAITNS	650+250, wait_zero_time
			//SLEEPNS 250, 1, wait_zero_time

			// turn off all the zero bits
			SBBO	gpio0_zeros, tmp0, 0, 4
#ifdef USES_GPIO1
			SBBO	gpio1_zeros, tmp1, 0, 4
#endif
			SBBO	gpio2_zeros, tmp2, 0, 4
#ifdef USES_GPIO3
			SBBO	gpio3_zeros, tmp3, 0, 4
#endif

			// Wait until the length of the one bits
			// TODO: Fix WAITNS so it can incorporate both delays?
			WAITNS	650+600, wait_one_time
			SLEEPNS	500, 1, sleep_one_time  // Wait a little longer to fix the timing

			// Turn all the bits off
			SBBO	gpio0_led_mask, tmp0, 0, 4
#ifdef USES_GPIO1
			SBBO	gpio1_led_mask, tmp1, 0, 4
#endif
			SBBO	gpio2_led_mask, tmp2, 0, 4
#ifdef USES_GPIO3
			SBBO	gpio3_led_mask, tmp3, 0, 4
#endif

			QBNE	BIT_LOOP, bit_num, 0

		// The 48 RGB streams have been clocked out
		// Move to the next color component for each pixel
		ADD	data_addr, data_addr, OUTPUTS
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
