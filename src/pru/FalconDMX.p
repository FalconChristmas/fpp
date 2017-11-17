// \file
 /* WS281x LED strip driver for the BeagleBone Black.
 *
 * To stop, the ARM can write a 0xFF to the command, which will
 * cause the PRU code to exit.
 *
 *   8 pins on GPIO03: 14 15 16 17 19 21
 *  each pixel is stored in 4 bytes in the order GRBA (4th byte is ignored)
 * 
 */
 
// Output 40
#define ser1_gpio	3
#define ser1_pin	21
// Output 42
#define ser2_gpio	3
#define ser2_pin	19
// Output 43
#define ser3_gpio	3
#define ser3_pin	17
// Output 44
#define ser4_gpio	3
#define ser4_pin	15
// Output 45
#define ser5_gpio	3
#define ser5_pin	16
// Output 46
#define ser6_gpio	3
#define ser6_pin	14
// Output 47
#define ser7_gpio	3
#define ser7_pin	20
// Output 48
#define ser8_gpio	3
#define ser8_pin	18


.origin 0
.entrypoint START

#include "FalconWS281x.hp"

/** Mappings of the GPIO devices */
#define GPIO3		0x481AE000

/** Offsets for the clear and set registers in the devices */
#define GPIO_CLEARDATAOUT	0x190
#define GPIO_SETDATAOUT		0x194

/** Register map */
#define data_addr	          r0
#define data_len	          r1
#define gpio3_ones	        r2
#define gpio3_zeros   	    r3
#define bit_num		          r4
#define sleep_counter	      r5

// r10 - r22 are used for temp storage and bitmap processing
#define gpio3_serial_mask	  r26


/** Sleep a given number of nanoseconds with 10 ns resolution.
 *
 * This busy waits for a given number of cycles.  Not for use
 * with things that must happen on a tight schedule.
 */
.macro SLEEPNS
.mparam ns,inst,lab
MOV sleep_counter, (ns/10)-1-inst // ws2811 -- high speed
lab:
	SUB sleep_counter, sleep_counter, 1
	QBNE lab, sleep_counter, 0
.endm

.macro WAITNS
.mparam ns,lab
	MOV r6, 0x22000 // control register
	// Instructions take 5ns and RESET_COUNTER takes about 20 instructions
	// this value was found through trial and error on the DMX signal
	// generation
	MOV r8, (ns)/5 - 20
lab:
	LBBO r7, r6, 0xC, 4 // read the cycle counter
	QBGT lab, r7, r8
.endm

.macro RESET_COUNTER
		// Disable the counter and clear it, then re-enable it
		MOV r6, 0x22000 // control register
		LBBO r9, r6, 0, 4
		CLR r9, r9, 3 // disable counter bit
		SBBO r9, r6, 0, 4 // write it back

		MOV r8, 0
		SBBO r8, r6, 0xC, 4 // clear the timer

		SET r9, r9, 3 // enable counter bit
		SBBO r9, r6, 0, 4 // write it back
.endm

#define CAT3(X,Y,Z) X##Y##Z
#define GPIO_MASK(X) CAT3(gpio,X,_serial_mask)
#define GPIO(R)	CAT3(gpio,R,_zeros)

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
//  MOV gpio3_serial_mask, #0
	SET	GPIO_MASK(ser1_gpio), ser1_pin
	SET	GPIO_MASK(ser2_gpio), ser2_pin
	SET	GPIO_MASK(ser3_gpio), ser3_pin
	SET	GPIO_MASK(ser4_gpio), ser4_pin
	SET	GPIO_MASK(ser5_gpio), ser5_pin
	SET	GPIO_MASK(ser6_gpio), ser6_pin
	SET	GPIO_MASK(ser7_gpio), ser7_pin
	SET	GPIO_MASK(ser8_gpio), ser8_pin

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

  MOV data_len, #513

  // Debug
  MOV	r13, GPIO3 | GPIO_CLEARDATAOUT
  MOV	r14, GPIO3 | GPIO_SETDATAOUT
  MOV r15, #0
  SET r15, #21
  SBBO	r15, r14, 0, 4
  SBBO	r15, r13, 0, 4
  
  MOV	r13, GPIO3 | GPIO_CLEARDATAOUT
  SBBO	gpio3_serial_mask, r13, 0, 4
  // Break Send Low for > 88us
  RESET_COUNTER
  WAITNS	90000, wait_break_low

 // End Break/Start MAB Send High for > 8us
  MOV	r13, GPIO3 | GPIO_SETDATAOUT
  SBBO	gpio3_serial_mask, r13, 0, 4
  RESET_COUNTER
  WAITNS	15000, wait_mab_high
 
  
 WORD_LOOP:
		// 11 bits (1 start, 8 data, 2 stop) 
		MOV	bit_num, 11
    // Load 8 bytes of data, starting at r10
		// one byte for each of the outputs
		LBBO	r10, data_addr, 0, 8
    RESET_COUNTER
  BIT_LOOP:
    QBEQ IS_START_BIT, bit_num, #11     // bit_num = 0 or 1 (Start bit)
    QBGT IS_STOP_BIT, bit_num, #3       // bit_num = 1 or 2 (Stop bits)
    MOV gpio3_ones, #0                  // Set all bit data low to start with
    MOV r12,#10
    SUB r12,r12,bit_num
  	//QBBC SET1, r10.b0, r12
    //SET gpio3_ones,#ser1_pin
  //SET1:
  	QBBC SET2, r10.b1, r12
    SET gpio3_ones,#ser2_pin
  SET2:
  	QBBC SET3, r10.b2, r12
    SET gpio3_ones,#ser3_pin
  SET3:
  	QBBC SET4, r10.b3, r12
    SET gpio3_ones,#ser4_pin
  SET4:
  	QBBC SET5, r11.b0, r12
    SET gpio3_ones,#ser5_pin
  SET5:
  	QBBC SET6, r11.b1, r12
    SET gpio3_ones,#ser6_pin
  SET6:
  	QBBC SET7, r11.b2, r12
    SET gpio3_ones,#ser7_pin
  SET7:
  	QBBC SET8, r11.b3, r12  
    SET gpio3_ones,#ser8_pin

  SET8:
    XOR gpio3_zeros, gpio3_ones, gpio3_serial_mask
    JMP WAIT_BIT    
  IS_START_BIT:
    MOV	gpio3_zeros, gpio3_serial_mask
    MOV	gpio3_ones, #0
    JMP WAIT_BIT    
  IS_STOP_BIT:
    MOV	gpio3_zeros, #0
    MOV	gpio3_ones, gpio3_serial_mask
    JMP WAIT_BIT    
  WAIT_BIT:
    MOV	r13, GPIO3 | GPIO_CLEARDATAOUT
    MOV	r14, GPIO3 | GPIO_SETDATAOUT
    WAITNS 3900, wait_bit1
    RESET_COUNTER
     // Output zero bits
    SBBO	gpio3_zeros, r13, 0, 4
    SBBO	gpio3_ones, r14, 0, 4

    SUB	bit_num, bit_num, 1
	  QBNE	BIT_LOOP, bit_num, 0

		ADD	data_addr, data_addr, 8
		SUB	data_len, data_len, 1
		QBNE	WORD_LOOP, data_len, #0

	// Write out that we are done!
	// Store a non-zero response in the buffer so that they know that we are done
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
