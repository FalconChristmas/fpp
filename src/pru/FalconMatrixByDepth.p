/**
 * Output data by bit depth.  For a given bit depth, all rows are
 * output before moving to the next bit depth.
 */

MOV bright, BITS
ROW_LOOP:
    // Reset the latch pin; will be toggled at the end of the row
    LATCH_LO
    CHECK_FOR_DISPLAY_OFF

    MOV pixelCount, ROW_LEN
    NEXT_PIXEL:
        // Load the 8 GPIO bit flags (4 for each of 2 pixels)
        // consecutive registers, starting at pixel_data
        // we do this by transfering the data_addr to the other PRU via XOUT
        // which will LBBO the data and then transfer it back via XIN
        REREAD:
            CHECK_FOR_DISPLAY_OFF
            XIN 11, data_from_other_pru, (32 + 4)
            QBNE REREAD, data_from_other_pru, data_addr
        ADD data_addr, data_addr, 32
        // XOUT the new data_addr so the other RPU can start working
        // on loading it while we process this data
        XOUT 10, data_addr, 4

        CLOCK_LO
        OUTPUT_GPIOS r18, r19, r20, r21
        CLOCK_HI

        CLOCK_LO
        OUTPUT_GPIOS r22, r23, r24, r25
        CLOCK_HI
        CHECK_FOR_DISPLAY_OFF

        SUB pixelCount, pixelCount, 2;
        QBEQ DONE_PIXELS, pixelCount, 0;
        JMP NEXT_PIXEL
    DONE_PIXELS:


#ifdef USING_PWM
        DISPLAY_OFF
#else
        QBEQ DISPLAY_ALREADY_OFF, sleep_counter, 0
        WAIT_FOR_TIMER:
            GET_PRU_CLOCK tmp_reg1, tmp_reg2
            QBGT WAIT_FOR_TIMER, tmp_reg1, sleep_counter
            DISPLAY_OFF
        DISPLAY_ALREADY_OFF:
#endif

        OUTPUT_ROW_ADDRESS

	    // Full data has been clocked out; latch it
	    LATCH_HI

        // determine on time (tmp_reg1) and delay time (tmp_reg2)
        GET_ON_DELAY_TIMES bright
        QBEQ NO_EXTRA_DELAY, tmp_reg2, 0
            MOV sleep_counter, tmp_reg2
            WAIT_FOR_EXTRA_OFF_TIME:
                GET_PRU_CLOCK tmp_reg2, tmp_reg3
                QBGT WAIT_FOR_EXTRA_OFF_TIME, tmp_reg2, sleep_counter
        NO_EXTRA_DELAY:

        RESET_PRU_CLOCK tmp_reg3, tmp_reg4

        MOV sleep_counter, tmp_reg1
        MOV sleepDone, 0

        DISPLAY_ON

        ADD row, row, 1
        QBNE ROW_LOOP, row, ROWS

        MOV row, 0
		// Update the brightness, and then give the row another scan
		SUB bright, bright, 1

		QBNE ROW_LOOP, bright, 0

        JMP READ_LOOP


