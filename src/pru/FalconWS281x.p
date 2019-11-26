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
 *  Reset is 300 usec
 */


// while len > 0:
// for bit# = 24 down to 0:
// delay 600 ns
// read 12 registers of data, build zero map for gpio0
// read 10 registers of data, build zero map for gpio1
// read  5 registers of data, build zero map for gpio3
//
// Send start pulse on all pins on gpio's
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

// defines are slightly lower as
// there is overhead in resetting the clocks
#define T0_TIME    230
#define T1_TIME   650
#define LOW_TIME  650

// GPIO0 sometimes takes a while to come out of sleep or
// something so use an lower "low" time
#define T0_TIME_GPIO0      180
#define T1_TIME_GPIO0     T1_TIME
#define LOW_TIME_GPIO0    LOW_TIME

#ifndef RUNNING_ON_PRU0
#define RUNNING_ON_PRU1
#endif

#if __has_include("/tmp/PinConfiguration.hp")
# include "/tmp/PinConfiguration.hp"
#endif
#include "FalconUtils.hp"

//register allocations for data
#define o1_dreg  r10.b0
#define o2_dreg  r10.b1
#define o3_dreg  r10.b2
#define o4_dreg  r10.b3
#define o5_dreg  r11.b0
#define o6_dreg  r11.b1
#define o7_dreg  r11.b2
#define o8_dreg  r11.b3
#define o9_dreg  r12.b0
#define o10_dreg  r12.b1
#define o11_dreg  r12.b2
#define o12_dreg  r12.b3
#define o13_dreg  r13.b0
#define o14_dreg  r13.b1
#define o15_dreg  r13.b2
#define o16_dreg  r13.b3
#define o17_dreg  r14.b0
#define o18_dreg  r14.b1
#define o19_dreg  r14.b2
#define o20_dreg  r14.b3
#define o21_dreg  r15.b0
#define o22_dreg  r15.b1
#define o23_dreg  r15.b2
#define o24_dreg  r15.b3
#define o25_dreg  r16.b0
#define o26_dreg  r16.b1
#define o27_dreg  r16.b2
#define o28_dreg  r16.b3
#define o29_dreg  r17.b0
#define o30_dreg  r17.b1
#define o31_dreg  r17.b2
#define o32_dreg  r17.b3
#define o33_dreg  r18.b0
#define o34_dreg  r18.b1
#define o35_dreg  r18.b2
#define o36_dreg  r18.b3
#define o37_dreg  r19.b0
#define o38_dreg  r19.b1
#define o39_dreg  r19.b2
#define o40_dreg  r19.b3
#define o41_dreg  r20.b0
#define o42_dreg  r20.b1
#define o43_dreg  r20.b2
#define o44_dreg  r20.b3
#define o45_dreg  r21.b0
#define o46_dreg  r21.b1
#define o47_dreg  r21.b2
#define o48_dreg  r21.b3


.origin 0
.entrypoint START

#include "FalconPRUDefs.hp"


/** Register map */
#define data_addr	r0
#define data_len	r1.w0
#define cur_data    r1.w2
#define gpio0_zeros	r2
#define gpio1_zeros	r3
#define gpio2_zeros	r4
#define gpio3_zeros	r5
#define bit_num     r6.b0
#define bit_flags   r6.b1
#define stats_time  r6.w2
#define sram_offset r7.w0
#define next_check  r7.w2
//r8 and r9 are used in WAITNS, can be used for other outside of that
//r10 - r21 are used for data storage and bitmap processing

//gpio address registers
#define gpio0_address   r22
#define gpio1_address   r23
#define gpio2_address   r24
#define gpio3_address   r25

#define gpio0_led_mask	r26
#define gpio1_led_mask	r27
#define gpio2_led_mask	r28
#define gpio3_led_mask	r29


#include "FalconWS281xOutputs.hp"

#ifdef RUNNING_ON_PRU1
#define SCRATCH_PAD 11
#else
#define SCRATCH_PAD 10
#endif

#if defined(USES_GPIO0) && !defined(SPLIT_GPIO0)
#define T0_TIME_PASS1    T0_TIME_GPIO0
#define T1_TIME_PASS1    T1_TIME_GPIO0
#define LOW_TIME_PASS1   LOW_TIME_GPIO0
#else
#define T0_TIME_PASS1    T0_TIME
#define T1_TIME_PASS1    T1_TIME
#define LOW_TIME_PASS1   LOW_TIME
#endif


.macro DISABLE_GPIO_PIN_INTERRUPTS
.mparam ledMask, gpio
    MOV r10, ledMask
    MOV r11, ledMask
    MOV r12, gpio
    MOV r13, 0x100
    SUB r12, r12, r13
    SBBO r10, r12, 0x3C, 8    //0x3c is the GPIO_IRQSTATUS_CLR_0 register
    // by doing 8 and using both r10 and r11, we can clear
    // both the 0 and 1 IRQ status
    MOV r10, 0
    ADD r12, r12, r13         // set clock to highest speed
    SBBO r10, r12, 0x30, 4    //0x30 is the GPIO_CTRL register
.endm
.macro DISABLE_PIN_INTERRUPTS
    DISABLE_GPIO_PIN_INTERRUPTS gpio0_led_mask, GPIO0
    DISABLE_GPIO_PIN_INTERRUPTS gpio1_led_mask, GPIO1
    DISABLE_GPIO_PIN_INTERRUPTS gpio2_led_mask, GPIO2
    DISABLE_GPIO_PIN_INTERRUPTS gpio3_led_mask, GPIO3

#ifdef USES_GPIO0
    // also need to turn off the GPIO0 idle and wakeup domain stuff
    MOV r12, GPIO0
    MOV r13, 0x100
    SUB r12, r12, r13
    LBBO r10, r12, 0x10, 4    //0x10 is the GPIO_SYSCONFIG register
    CLR r10, 0     //AUTOIDLE
    CLR r10, 2     //ENAWAKEUP
    SET r10, 3     //No-Idle
    CLR r10, 4     //
    SBBO r10, r12, 0x10, 4    //0x10 is the GPIO_SYSCONFIG register


    MOV r9, 0x44E00500
    LDI r10, 2
    SBBO r10, r9, 0x3c, 4     //use the accurate clock

    MOV r9, 0x44E00400        //CM_WKUP registers
    LBBO r10, r13, 0x08, 4    //0x08 is the CM_WKUP_GPIO0_CLKCTRL register
    SET r10, 1     //ENABLE
    CLR r10, 0     //ENABLE
    CLR r10, 16     //IDLEST
    CLR r10, 17     //IDLEST
    SET r10, 18
    SBBO r10, r13, 0x08, 4
    
    LBBO r10, r13, 0x00, 4    //0x00 is the CM_WKUP_CLKSTCTRL register
    CLR r10, 0
    CLR r10, 1
    SET r10, 2
    SET r10, 8
    SBBO r10, r13, 0x00, 4
    
    LBBO r10, r13, 0x04, 4    //0x04 is the CM_WKUP_CONTROL_CLKCTRL register
    CLR r10, 17
    CLR r10, 16
    SET r10, 1
    CLR r10, 0
    SBBO r10, r13, 0x04, 4

    LBBO r10, r13, 0x0C, 4    //0x04 is the CM_WKUP_L4WKUP_CLKCTRL register
    CLR r10, 17
    CLR r10, 16
    SET r10, 1
    CLR r10, 0
    SBBO r10, r13, 0x0C, 4    //0x10 is the SYSCONFIG register
    
    MOV r9, 0x44E00400        //CM_MPU registers
    LBBO r10, r13, 0x00, 8    //0x00 is the CM_MPU_CLKSTCTRL register
    CLR r10, 0
    CLR r10, 1
    CLR r11, 0
    SET r11, 1
    SBBO r10, r13, 0x00, 8

    MOV r9, 0x44E00000        //CM_PER registers
    LBBO r10, r13, 0x00, 8    //0x00 is the CM_PER_L4LS_CLKSTCTRL register
    CLR r10, 0
    CLR r10, 1
    CLR r11, 0
    CLR r11, 1
    SBBO r10, r13, 0x00, 8
    LBBO r10, r13, 0x0C, 4    //0x0C is the CM_PER_L3_CLKSTCTRL register
    CLR r10, 0
    CLR r10, 1
    CLR r11, 0
    CLR r11, 1
    SBBO r10, r13, 0x00, 4

    

    MOV r13, 0x50000000       //GPMC registers
    LBBO r10, r13, 0x10, 4    //0x10 is the SYSCONFIG register
    CLR r10, 0     //AUTOIDLE
    CLR r10, 2     //ENAWAKEUP
    SET r10, 3     //No-Idle
    CLR r10, 4     //
    SBBO r10, r13, 0x10, 4    //0x10 is the SYSCONFIG register
#endif
.endm

.macro CLEAR_IF_NOT_EQUAL
.mparam  val, gpioAdd, equ
QBEQ skip, val, equ
    SBBO    val, gpioAdd, GPIO_CLRDATAOUT, 4
skip:
.endm

.macro SET_IF_NOT_EQUAL
.mparam  val, gpioAdd, equ
QBEQ skip, val, equ
SBBO    val, gpioAdd, GPIO_SETDATAOUT, 4
skip:
.endm


.macro READ_DATA
    // Load OUTPUTS bytes of data, starting at r10
    // one byte for each of the outputs
    QBBS USEDDR, bit_flags.t3
    QBBS USESHAREDRAM,  bit_flags.t2

    QBBS USERAM2, bit_flags.t1
#ifdef RUNNING_ON_PRU1
        LBCO    r10, CONST_PRUDRAM, sram_offset, OUTPUTS
#else
        LBCO    r10, CONST_OTHERPRUDRAM, sram_offset, OUTPUTS
#endif
        ADD     sram_offset, sram_offset, OUTPUTS
        MOV     r8, 8142 //8k - 50
        QBLT DATALOADED, r8, sram_offset
            //reached the end of what we have in our sram, flip to other SRAM
            MOV r8, 7628
            SUB sram_offset, sram_offset, r8
            SET bit_flags.t1
            QBA DATALOADED
    USERAM2:
#ifdef RUNNING_ON_PRU1
        LBCO    r10, CONST_OTHERPRUDRAM, sram_offset, OUTPUTS
#else
        LBCO    r10, CONST_PRUDRAM, sram_offset, OUTPUTS
#endif

        ADD     sram_offset, sram_offset, OUTPUTS
        MOV     r8, 8142 //8k - 50
        QBLT    DATALOADED, r8, sram_offset
            //reached the end of what we have in other sram, flip to sharedram
            MOV r8, (7628 + 512)
            SUB sram_offset, sram_offset, r8
            SET bit_flags.t2
        QBA     DATALOADED
    USESHAREDRAM:
        MOV     r9, 0x00010000
        LBBO    r10, r9, sram_offset, OUTPUTS
        ADD     sram_offset, sram_offset, OUTPUTS
        MOV     r8, 12188
        QBLT DATALOADED, r8, sram_offset
            //reached the end of what we have in other sram, flip to DDR
            SET bit_flags.t3
            QBA     DATALOADED
    USEDDR:
        LBBO    r10, data_addr, 0, OUTPUTS
    DATALOADED:
    ADD data_addr, data_addr, OUTPUTS
.endm

.macro WAIT_AND_CHECK_TIMEOUT
.mparam TIMEOUT, reg1, reg2, timeoutLabel
    // need to subtract 2 clock cycles (10ns) for the MOVE/QBGT atfer the WAITNS
    WAITNS    (TIMEOUT - 10), reg1, reg2
    MOV reg1, ((TIMEOUT+50)/5)
    QBGT timeoutLabel, reg1, reg2
.endm


#if !defined(FIRST_CHECK)
#define FIRST_CHECK NO_PIXELS_CHECK
#endif

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
	//MOV	r0, 0x00000120
	//MOV	r1, CTPPR_0 + PRU_MEMORY_OFFSET
	//ST32	r0, r1

	// Configure the programmable pointer register for PRU by setting
	// c31_pointer[15:0] field to 0x0010.  This will make C31 point to
	// 0x80001000 (DDR memory).
	MOV	r0, 0x00100000
	MOV	r1, CTPPR_1 + PRU_MEMORY_OFFSET
	ST32	r0, r1

	// Write a 0x1 into the response field so that they know we have started
	MOV	r2, #0x1
	SBCO	r2, CONST_PRUDRAM, 8, 4

    MOV gpio0_address, GPIO0
    MOV gpio1_address, GPIO1
    MOV gpio2_address, GPIO2
    MOV gpio3_address, GPIO3

    LDI gpio0_led_mask, 0
    LDI gpio1_led_mask, 0
    LDI gpio2_led_mask, 0
    LDI gpio3_led_mask, 0

    SETOUTPUT1MASK
    SETOUTPUT2MASK
    SETOUTPUT3MASK
    SETOUTPUT4MASK
    SETOUTPUT5MASK
    SETOUTPUT6MASK
    SETOUTPUT7MASK
    SETOUTPUT8MASK
    SETOUTPUT9MASK
    SETOUTPUT10MASK
    SETOUTPUT11MASK
    SETOUTPUT12MASK
    SETOUTPUT13MASK
    SETOUTPUT14MASK
    SETOUTPUT15MASK
    SETOUTPUT16MASK
    SETOUTPUT17MASK
    SETOUTPUT18MASK
    SETOUTPUT19MASK
    SETOUTPUT20MASK
    SETOUTPUT21MASK
    SETOUTPUT22MASK
    SETOUTPUT23MASK
    SETOUTPUT24MASK
    SETOUTPUT25MASK
    SETOUTPUT26MASK
    SETOUTPUT27MASK
    SETOUTPUT28MASK
    SETOUTPUT29MASK
    SETOUTPUT30MASK
    SETOUTPUT31MASK
    SETOUTPUT32MASK
    SETOUTPUT33MASK
    SETOUTPUT34MASK
    SETOUTPUT35MASK
    SETOUTPUT36MASK
    SETOUTPUT37MASK
    SETOUTPUT38MASK
    SETOUTPUT39MASK
    SETOUTPUT40MASK
    SETOUTPUT41MASK
    SETOUTPUT42MASK
    SETOUTPUT43MASK
    SETOUTPUT44MASK
    SETOUTPUT45MASK
    SETOUTPUT46MASK
    SETOUTPUT47MASK
    SETOUTPUT48MASK

    // save the led masks to the scratch pad as we'll modify these during output
    XOUT SCRATCH_PAD, gpio0_led_mask, 16

    DISABLE_PIN_INTERRUPTS

	// Wait for the start condition from the main program to indicate
	// that we have a rendered frame ready to clock out.  This also
	// handles the exit case if an invalid value is written to the start
	// start position.
_LOOP:
    //make sure the clock starts
    RESET_PRU_CLOCK r8, r9

	// Load the pointer to the buffer from PRU DRAM into r0 and the
	// start command into r1
	LBCO	data_addr, CONST_PRUDRAM, 0, 8

	// Wait for a non-zero command
	QBEQ	_LOOP, r1, #0

	// Command of 0xFF is the signal to exit
	QBEQ	EXIT, r1, #0xFF

    // store the address and such
    XOUT    SCRATCH_PAD, data_addr, 12

    RESET_PRU_CLOCK r8, r9

	MOV	data_len, DATA_LEN

    MOV sram_offset, 512
    LDI bit_flags, 0
    LDI cur_data, 0
    LDI next_check, #FIRST_CHECK

    //restore the led masks
    XIN SCRATCH_PAD, gpio0_led_mask, 16

    // reset command to 0 so ARM side will send more data
    LDI     r2, 0
    MOV     r3, 1
    SBCO    r2, CONST_PRUDRAM, 4, 8
    
    //start the clock
    RESET_PRU_CLOCK r8, r9

	WORD_LOOP:
    LOOP WORD_LOOP_DONE, data_len
        READ_DATA

		// for bit in 8 to 0; one color at a time
		MOV	bit_num, 8
#ifdef RECORD_STATS
        LDI stats_time, 0
#endif

		BIT_LOOP:
			SUB     bit_num, bit_num, 1
			// The idle period is 650 ns, but this is where
			// we do all of our work to read the RGB data and
			// repack it into bit slices.  Read the current counter
			// and then wait until 650 ns have passed once we complete
			// our work.

            MOV gpio0_zeros, gpio0_led_mask
            MOV gpio1_zeros, gpio1_led_mask
            MOV gpio2_zeros, gpio2_led_mask
            MOV gpio3_zeros, gpio3_led_mask

#if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
            DO_OUTPUT_GPIO0
#endif
            DO_OUTPUT_GPIO1
            DO_OUTPUT_GPIO2
            DO_OUTPUT_GPIO3

#ifdef RECORD_STATS

    QBLT NOTMORE, data_len, 128
    GET_PRU_CLOCK r8, r9
    QBLT NOTMORE, stats_time, r8
        MOV stats_time, r8
        ADD r8, data_len, data_len
        ADD r8, r8, 12
        SBCO stats_time, CONST_PRUDRAM, r8, 2

    NOTMORE:
#endif

            //wait for the full cycle to complete
            WAIT_AND_CHECK_TIMEOUT  LOW_TIME_PASS1, r8, r9, WORD_LOOP_DONE

            //start the clock
            RESET_PRU_CLOCK r8, r9

			// Send all the start bits
#ifdef USES_GPIO1
            SET_IF_NOT_EQUAL gpio1_led_mask, gpio1_address, 0
#endif
#ifdef USES_GPIO2
            SET_IF_NOT_EQUAL gpio2_led_mask, gpio2_address, 0
#endif
#ifdef USES_GPIO3
            SET_IF_NOT_EQUAL gpio3_led_mask, gpio3_address, 0
#endif
#if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
            SET_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, 0
#endif

#ifdef USES_GPIO1
            AND gpio1_zeros, gpio1_zeros, gpio1_led_mask
#endif
#ifdef USES_GPIO2
            AND gpio2_zeros, gpio2_zeros, gpio2_led_mask
#endif
#ifdef USES_GPIO3
            AND gpio3_zeros, gpio3_zeros, gpio3_led_mask
#endif
#if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
            AND gpio0_zeros, gpio0_zeros, gpio0_led_mask
#endif

			// wait for the length of the zero bits
            WAIT_AND_CHECK_TIMEOUT  T0_TIME_PASS1, r8, r9, WORD_LOOP_DONE

            // turn off all the zero bits
            // if gpio_zeros is 0, nothing will be turned off, skip
#ifdef USES_GPIO1
            CLEAR_IF_NOT_EQUAL  gpio1_zeros, gpio1_address, 0
#endif
#ifdef USES_GPIO2
            CLEAR_IF_NOT_EQUAL  gpio2_zeros, gpio2_address, 0
#endif
#ifdef USES_GPIO3
            CLEAR_IF_NOT_EQUAL  gpio3_zeros, gpio3_address, 0
#endif
#if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
            CLEAR_IF_NOT_EQUAL  gpio0_zeros, gpio0_address, 0
#endif

			// Wait until the length of the one bits
			WAIT_AND_CHECK_TIMEOUT	T1_TIME_PASS1, r8, r9, WORD_LOOP_DONE

            // Turn all the bits off
            // if gpio#_zeros is equal to the led mask, then everythin was
            // already shut off, don't output
#ifdef USES_GPIO1
            CLEAR_IF_NOT_EQUAL gpio1_led_mask, gpio1_address, gpio1_zeros
#endif
#ifdef USES_GPIO2
            CLEAR_IF_NOT_EQUAL gpio2_led_mask, gpio2_address, gpio0_zeros
#endif
#ifdef USES_GPIO3
            CLEAR_IF_NOT_EQUAL gpio3_led_mask, gpio3_address, gpio3_zeros
#endif
#if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
            CLEAR_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, gpio0_zeros
#endif
            //start the clock for the LOW time
            RESET_PRU_CLOCK r8, r9

			QBNE	BIT_LOOP, bit_num, 0

		// The RGB streams have been clocked out
		// Move to the next color component for each pixel
        ADD     cur_data, cur_data, 1
        CALL    next_check
#ifdef RECORD_STATS
        SUB        data_len, data_len, 1
#endif
		//  QBNE	WORD_LOOP, data_len, #0
    WORD_LOOP_DONE:
    
    
    // Turn all the bits off
    #ifdef USES_GPIO1
                CLEAR_IF_NOT_EQUAL gpio1_led_mask, gpio1_address, 0
    #endif
    #ifdef USES_GPIO2
                CLEAR_IF_NOT_EQUAL gpio2_led_mask, gpio2_address, 0
    #endif
    #ifdef USES_GPIO3
                CLEAR_IF_NOT_EQUAL gpio3_led_mask, gpio3_address, 0
    #endif
    #if !defined(SPLIT_GPIO0) && defined(USES_GPIO0)
                CLEAR_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, 0
    #endif
    

#if defined(SPLIT_GPIO0) && defined(USES_GPIO0)
    // do a second pass for GPIO0 only

    // restore the address and such
    XIN    SCRATCH_PAD, data_addr, 12

    RESET_PRU_CLOCK r8, r9
	MOV	data_len, DATA_LEN

    MOV sram_offset, 512
    LDI bit_flags, 0
    LDI cur_data, 0
    LDI next_check, #FIRST_CHECK

    //restore the led masks
    XIN SCRATCH_PAD, gpio0_led_mask, 16

	WORD_LOOP_PASS2:
    LOOP WORD_LOOP_DONE_PASS2, data_len
        READ_DATA

		// for bit in 8 to 0; one color at a time
		MOV	bit_num, 8
		BIT_LOOP_PASS2:
			SUB     bit_num, bit_num, 1
			// The idle period is 650 ns, but this is where
			// we do all of our work to read the RGB data and
			// repack it into bit slices.  Read the current counter
			// and then wait until 650 ns have passed once we complete
			// our work.

            MOV gpio0_zeros, gpio0_led_mask
            DO_OUTPUT_GPIO0

            //wait for the full cycle to complete
            WAIT_AND_CHECK_TIMEOUT    LOW_TIME_GPIO0, r8, r9, WORD_LOOP_DONE_PASS2

            //start the clock
            RESET_PRU_CLOCK r8, r9

            SET_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, 0
            AND gpio0_zeros, gpio0_zeros, gpio0_led_mask

			// wait for the length of the zero bits
            WAIT_AND_CHECK_TIMEOUT    T0_TIME_GPIO0, r8, r9, WORD_LOOP_DONE_PASS2

            // turn off all the zero bits
            // if gpio_zeros is 0, nothing will be turned off, skip
            CLEAR_IF_NOT_EQUAL  gpio0_zeros, gpio0_address, 0

			// Wait until the length of the one bits
			WAIT_AND_CHECK_TIMEOUT	T1_TIME_GPIO0, r8, r9, WORD_LOOP_DONE_PASS2

            // Turn all the bits off
            // if gpio#_zeros is equal to the led mask, then everythin was
            // already shut off, don't output
            CLEAR_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, gpio0_zeros

            //start the clock for the LOW time
            RESET_PRU_CLOCK r8, r9

			QBNE	BIT_LOOP_PASS2, bit_num, 0

		// The RGB streams have been clocked out
		// Move to the next color component for each pixel
        ADD     cur_data, cur_data, 1
        CALL    next_check
		//  QBNE	WORD_LOOP_PASS2, data_len, #0
    WORD_LOOP_DONE_PASS2:
    CLEAR_IF_NOT_EQUAL gpio0_led_mask, gpio0_address, 0

#endif   // GPIO0 second pass

	// Delay at least 300 usec; this is the required reset
	// time for the LED strip to update with the new pixels.
	SLEEPNS	300000, r8

	// Write out that we are done!
	// Store a non-zero response in the buffer so that they know that we are done
        // and zero out the command
	// LDI	    r2, 0
        // MOV     r3, 1
        // SBCO	r2, CONST_PRUDRAM, 4, 8

	// Go back to waiting for the next frame buffer
	QBA	_LOOP

EXIT:
	// Write a 0xFFFF into the response field so that they know we're done
	MOV r2, #0xFFFF
	SBCO r2, CONST_PRUDRAM, 8, 4

#ifdef AM33XX
	// Send notification to Host for program completion
	MOV R31.b0, PRU_ARM_INTERRUPT+16
#else
	MOV R31.b0, PRU_ARM_INTERRUPT
#endif

	HALT



NO_PIXELS_CHECK:
    RET

#if __has_include("/tmp/OutputLengths.hp")
#include "/tmp/OutputLengths.hp"
#endif
