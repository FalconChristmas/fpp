/* WS281x LED strip driver for the BeagleBone Black.
 *
 * To stop, the ARM can write a 0xFF to the command, which will
 * cause the PRU code to exit.
 *
 *   8 pins on GPIO03: 14 15 16 17 19 21
 *  each pixel is stored in 4 bytes in the order GRBA (4th byte is ignored)
 * 
 */

#include "FalconSerial.hp"


.origin 0
.entrypoint START

#include "FalconWS281x.hp"
#include "FalconUtils.hp"

#ifdef PIXELNET
//PixelNet
#define DELAYCOUNT 990
#define DATALEN    #4102

#else
#define DELAYCOUNT 3900

#ifndef DATALEN
#define DATALEN    #513
#endif

// DMX has to send SOMETHING out at least every 250ms (we'll do slightly faster) or
// some controllers lose the signal and go into a test loop
// each loop is 4 instructions, one is an LBCO of 12 bytes which takes 45 cycles so 48 cycles total
// 200ms * 5ns/cycle * 200mhz
#define CONST_MAX_BETWEEN_FRAME 0xC0000

/// There is 8K of ram which COULD be used for DMX, but dmx has plenty of time to go
/// off to main ram so we'll let the ws2811 code use the ram
///  #define USING_PRU_RAM
#endif


/** Mappings of the GPIO devices */
#define GPIO3		0x481AE000

/** Offsets for the clear and set registers in the devices */
#define GPIO_CLEARDATAOUT	0x190
#define GPIO_SETDATAOUT		0x194

/** Register map */
#define data_addr	          r0
#define data_len	          r1
#define bit_num		          r2
#define sleep_counter	      r3

// r10 - r22 are used for temp storage and bitmap processing

.macro RESET_COUNTER
		// Disable the counter and clear it, then re-enable it
		MOV r6, PRU_CONTROL_REG // control register
		LBBO r9, r6, 0, 4
		CLR r9, r9, 3 // disable counter bit
		SBBO r9, r6, 0, 4 // write it back

		MOV r8, 0
		SBBO r8, r6, 0xC, 4 // clear the timer

		SET r9, r9, 3 // enable counter bit
		SBBO r9, r6, 0, 4 // write it back
.endm

#ifdef RUNNING_ON_PRU1
#define USE_SLOW_GPIO
#endif

#ifdef USE_SLOW_GPIO
#define gpio3_zeros   	      r4
#define gpio3_ones            r5   //must be right after _zeros

#define clearDataOffset       r13
#define setDataOffset         r14
#define gpio3_serial_mask	  r26

.macro CLEAR_ALL_BITS
    SBBO    gpio3_serial_mask, clearDataOffset, 0, 4
.endm
.macro SET_ALL_BITS
    SBBO	gpio3_serial_mask, setDataOffset, 0, 4
.endm
.macro BEFORE_SET_BITS
    MOV gpio3_ones, #0       // Set all bit data low to start with
.endm
.macro AFTER_SET_BITS
    XOR gpio3_zeros, gpio3_ones, gpio3_serial_mask   // create the 0 bits
.endm
.macro WAIT_BEFORE_SET_BITS
.endm
.macro WAIT_AFTER_SET_BITS
    SLEEPNS DELAYCOUNT, sleep_counter, 20
.endm
.macro OUTPUT_GPIO_BITS
    // Output bits, write both 0's and 1's
    SBBO	gpio3_zeros, clearDataOffset, 0, 8
.endm
.macro OUTPUT_START_BIT
    MOV	gpio3_zeros, gpio3_serial_mask
    MOV	gpio3_ones, #0
.endm
.macro OUTPUT_STOP_BIT
    MOV	gpio3_zeros, #0
    MOV	gpio3_ones, gpio3_serial_mask
.endm
.macro DO_SET_BIT
.mparam pin, reg, bit
    QBBC NOT_SET, reg, bit
    SET gpio3_ones, #pin
    NOT_SET:
.endm
#define SET_BIT(out, reg, bit)  DO_SET_BIT ser##out##_pin, reg, bit
#else

.macro CLEAR_ALL_BITS
    CLR r30, ser1_pru30
    CLR r30, ser2_pru30
    CLR r30, ser3_pru30
    CLR r30, ser4_pru30
#if NUMOUT == 8
    CLR r30, ser5_pru30
    CLR r30, ser6_pru30
    CLR r30, ser7_pru30
    CLR r30, ser8_pru30
#endif
.endm
.macro SET_ALL_BITS
    SET r30, ser1_pru30
    SET r30, ser2_pru30
    SET r30, ser3_pru30
    SET r30, ser4_pru30
#if NUMOUT == 8
    SET r30, ser5_pru30
    SET r30, ser6_pru30
    SET r30, ser7_pru30
    SET r30, ser8_pru30
#endif
.endm
.macro BEFORE_SET_BITS
.endm
.macro AFTER_SET_BITS
.endm
.macro WAIT_BEFORE_SET_BITS
    SLEEPNS DELAYCOUNT, sleep_counter, 20
.endm
.macro WAIT_AFTER_SET_BITS
.endm
.macro OUTPUT_GPIO_BITS
.endm
.macro OUTPUT_START_BIT
    CLEAR_ALL_BITS
.endm
.macro OUTPUT_STOP_BIT
    SET_ALL_BITS
.endm

.macro DO_SET_BIT
.mparam pin, reg, bit
    QBBC NOT_SET, reg, bit
        SET r30, #pin
        JMP DONE
    NOT_SET:
        CLR r30, #pin
    DONE:
.endm
#define SET_BIT(out, reg, bit)  DO_SET_BIT ser##out##_pru30, reg, bit


#endif



#define CAT3(X,Y,Z) X##Y##Z
#define GPIO_MASK(X) CAT3(gpio,X,_serial_mask)
#define GPIO(R)	CAT3(gpio,R,_zeros)			\

START:
	// Enable OCP master port
	// clear the STANDBY_INIT bit in the SYSCFG register,
	// otherwise the PRU will not be able to write outside the
	// PRU memory space and to the BeagleBon's pins.
	LBCO	r0, C4, 4, 4
	CLR	r0, r0, 4
	SBCO	r0, C4, 4, 4

	// Configure the programmable pointer register for PRU by setting
	// c28_pointer[15:0] field to 0x0120.  This will make C28 point to
	// 0x00012000 (PRU shared RAM).
	MOV	r0, 0x00000120
	MOV	r1, CTPPR_0 + PRU_MEMORY_OFFSET
	ST32	r0, r1

	// Configure the programmable pointer register for PRU by setting
	// c31_pointer[15:0] field to 0x0010.  This will make C31 point to
	// 0x80001000 (DDR memory).
	MOV	r0, 0x00100000
	MOV	r1, CTPPR_1 + PRU_MEMORY_OFFSET
	ST32	r0, r1

	// Write a 0x1 into the response field so that they know we have started
	MOV	r2, #0x1
	SBCO	r2, CONST_PRUDRAM, 12, 4

#ifdef USE_SLOW_GPIO
    LDI gpio3_serial_mask, 0
	SET	GPIO_MASK(ser1_gpio), ser1_pin
	SET	GPIO_MASK(ser2_gpio), ser2_pin
	SET	GPIO_MASK(ser3_gpio), ser3_pin
	SET	GPIO_MASK(ser4_gpio), ser4_pin
#if NUMOUT == 8
	SET	GPIO_MASK(ser5_gpio), ser5_pin
	SET	GPIO_MASK(ser6_gpio), ser6_pin
	SET	GPIO_MASK(ser7_gpio), ser7_pin
	SET	GPIO_MASK(ser8_gpio), ser8_pin
#endif

    MOV clearDataOffset, GPIO3 | GPIO_CLEARDATAOUT
    MOV setDataOffset, GPIO3 | GPIO_SETDATAOUT
#endif

#ifdef CONST_MAX_BETWEEN_FRAME
    MOV sleep_counter, CONST_MAX_BETWEEN_FRAME
#endif

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
#ifdef CONST_MAX_BETWEEN_FRAME
    // more than 200ms since last out, need to re-output or signal
    // will be lost
    SUB     sleep_counter, sleep_counter, 1
    QBEQ    _OUTPUTANYWAY, sleep_counter, #0
#endif

	// Load the pointer to the buffer from PRU DRAM into r0 and the
	// length (in bytes-bit words) into r1.
	// start command into r2
	LBCO	data_addr, CONST_PRUDRAM, 0, 12

	// Wait for a non-zero command
	QBEQ	_LOOP, r2, #0

_OUTPUTANYWAY:

	// Zero out the start command so that they know we have received it
	// This allows maximum speed frame drawing since they know that they
	// can now swap the frame buffer pointer and write a new start command.
	MOV	r3, 0
	SBCO	r3, CONST_PRUDRAM, 8, 4

	// Command of 0xFF is the signal to exit
	QBEQ	EXIT, r2, #0xFF
    MOV data_len, DATALEN

#ifndef PIXELNET
    //DMX needs to sent a long 0
    CLEAR_ALL_BITS
    // Break Send Low for > 88us
    RESET_COUNTER
    SLEEPNS 90000, sleep_counter, 20

    // End Break/Start MAB Send High for > 8us
    SET_ALL_BITS
    RESET_COUNTER
    SLEEPNS 15000, sleep_counter, 20

    //DMX will use pru ram and not DDR
#ifdef USING_PRU_RAM
    MOV data_addr, 512
#endif
#endif
  
 WORD_LOOP:
	// 11 bits (1 start, 8 data, 2 stop)
	MOV	bit_num, 11
    // Load 8 bytes of data, starting at r10
	// one byte for each of the outputs

#ifdef USING_PRU_RAM
    LBCO    r10, CONST_PRUDRAM, data_addr, NUMOUT
#else
    LBBO    r10, data_addr, 0, NUMOUT
#endif
    RESET_COUNTER
    BIT_LOOP:
        WAIT_BEFORE_SET_BITS
        QBEQ IS_START_BIT, bit_num, #11     // bit_num = 0 or 1 (Start bit)
        QBGT IS_STOP_BIT, bit_num, #3       // bit_num = 1 or 2 (Stop bits)
        MOV r12,#10
        SUB r12,r12,bit_num

        BEFORE_SET_BITS

        SET_BIT(1, r10.b0, r12)
        SET_BIT(2, r10.b1, r12)
        SET_BIT(3, r10.b2, r12)
        SET_BIT(4, r10.b3, r12)
        #if NUMOUT == 8
            SET_BIT(5, r11.b0, r12)
            SET_BIT(6, r11.b1, r12)
            SET_BIT(7, r11.b2, r12)
            SET_BIT(8, r11.b3, r12)
        #endif

        AFTER_SET_BITS

        JMP WAIT_BIT
      IS_START_BIT:
        OUTPUT_START_BIT
        JMP WAIT_BIT
      IS_STOP_BIT:
        OUTPUT_STOP_BIT
        JMP WAIT_BIT
      WAIT_BIT:
        WAIT_AFTER_SET_BITS
        RESET_COUNTER

        OUTPUT_GPIO_BITS

        SUB     bit_num, bit_num, 1
        QBNE	BIT_LOOP, bit_num, 0

    ADD     data_addr, data_addr, 8
    SUB     data_len, data_len, 1
    QBNE    WORD_LOOP, data_len, #0

	// Write out that we are done!
	// Store a non-zero response in the buffer so that they know that we are done
	MOV     r8, PRU_CONTROL_REG // control register
	LBBO	r2, r8, 0xC, 4
	SBCO	r2, CONST_PRUDRAM, 12, 4

	// Go back to waiting for the next frame buffer
#ifdef CONST_MAX_BETWEEN_FRAME
    MOV sleep_counter, CONST_MAX_BETWEEN_FRAME
#endif
	QBA	_LOOP

EXIT:
	// Write a 0xFF into the response field so that they know we're done
	MOV r2, #0xFF
	SBCO r2, CONST_PRUDRAM, 12, 4

#ifdef AM33XX
	// Send notification to Host for program completion
	MOV R31.b0, PRU_ARM_INTERRUPT+16
#else
	MOV R31.b0, PRU_ARM_INTERRUPT
#endif

	HALT
