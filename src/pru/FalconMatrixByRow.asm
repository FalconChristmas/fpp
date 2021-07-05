
/**
 * Output data by row.  All the bit depths for the row are output before
 * moving onto the next row.  Works best for panels that my be slow
 * with flipping from row to row, but may have more flicker due to the
 * extended "off time" for each row when nothing it lit up
 */

NEW_ROW_LOOP:
    LDI bright, BITS

ROW_LOOP:
		; Reset the latch pin; will be toggled at the end of the row
		LATCH_LO
        CHECK_FOR_DISPLAY_OFF

        LDI pixelCount, ROW_LEN
NEXT_PIXEL:

	; Load the 8 GPIO bit flags (4 for each of 2 pixels)
	; consecutive registers, starting at pixel_data
        READ_DATA

            CLOCK_LO
            OUTPUT_GPIOS r18, r19, r20, r21
            CLOCK_HI

            CLOCK_LO
            OUTPUT_GPIOS r22, r23, r24, r25
            CLOCK_HI
            CHECK_FOR_DISPLAY_OFF

            SUB pixelCount, pixelCount, 2
            QBEQ DONE_PIXELS, pixelCount, 0
            QBA NEXT_PIXEL
DONE_PIXELS:

#ifdef ENABLESTATS
        //write some debug data into sram to read in c code
        MOV tmp_reg3, sleep_counter
        GET_PRU_CLOCK tmp_reg1, tmp_reg1, 8
        QBNE STILLON, sleep_counter, 0
            MOV tmp_reg3, sleepDone
STILLON:

        MOV tmp_reg4, statOffset
        LSL tmp_reg4, tmp_reg4, 2
        ADD tmp_reg4, tmp_reg4, 28
        SBCO &tmp_reg1, CONST_PRUDRAM, tmp_reg4, 12
        ADD statOffset, statOffset, 3
#endif

#ifdef USING_PWM
        DISPLAY_OFF
#else
        QBEQ DISPLAY_ALREADY_OFF, sleep_counter, 0
WAIT_FOR_TIMER:
            GET_PRU_CLOCK tmp_reg1, tmp_reg2, 4
            QBGT WAIT_FOR_TIMER, tmp_reg1, sleep_counter
            DISPLAY_OFF
DISPLAY_ALREADY_OFF:
#endif

        QBNE NO_SET_ROW, bright, BITS //maxBitsToOutput
            OUTPUT_ROW_ADDRESS
            READ_TO_FLUSH
            READ_TO_FLUSH
NO_SET_ROW:

	    ; Full data has been clocked out; latch it
	    LATCH_HI
        READ_TO_FLUSH


        ; determine on time (tmp_reg1) and delay time (tmp_reg2)
        GET_ON_DELAY_TIMES bright
        QBEQ NO_EXTRA_DELAY, tmp_reg2, 0
            MOV sleep_counter, tmp_reg2
WAIT_FOR_EXTRA_OFF_TIME:
                GET_PRU_CLOCK tmp_reg2, tmp_reg3, 4
                QBGT WAIT_FOR_EXTRA_OFF_TIME, tmp_reg2, sleep_counter
NO_EXTRA_DELAY:

        RESET_PRU_CLOCK tmp_reg3, tmp_reg4

        MOV sleep_counter, tmp_reg1
        LDI sleepDone, 0
        LATCH_LO
        READ_TO_FLUSH


        DISPLAY_ON

		; Update the brightness, and then give the row another scan
		SUB bright, bright, 1

		QBGE ROW_DONE, bright, 0
            QBA ROW_LOOP
ROW_DONE:

		; We have just done all eight brightness levels for this
		; row.  Time to move to the new row
        ADD row, row, 1

#ifdef OUTPUTBLANKROW
        ; Some panels need a fully blank row to prevent some ghosting
        ; we will do that as quickly as possible
        ZERO &r18, 16
        LDI pixelCount, ROW_LEN
        LATCH_LO
        READ_TO_FLUSH
NEXT_PIXEL_BLANK:
            CLOCK_LO
            OUTPUT_GPIOS r18, r19, r20, r21
            CLOCK_HI
            CHECK_FOR_DISPLAY_OFF

            SUB pixelCount, pixelCount, 1
            QBEQ DONE_PIXELS_BLANK, pixelCount, 0
            QBA NEXT_PIXEL_BLANK
DONE_PIXELS_BLANK:

WAIT_FOR_TIMER_BLANK:
        QBEQ DISPLAY_ALREADY_OFF_BLANK, sleep_counter, 0
            CHECK_FOR_DISPLAY_OFF
            QBA WAIT_FOR_TIMER_BLANK
DISPLAY_ALREADY_OFF_BLANK:

        LATCH_HI

        RESET_PRU_CLOCK tmp_reg3, tmp_reg4
        LDI sleep_counter, 20
        LDI sleepDone, 0
        LATCH_LO
        READ_TO_FLUSH

        DISPLAY_ON

#endif

        QBNE READ_LOOP_DONE, row, ROWS
            QBA READ_LOOP
READ_LOOP_DONE:

		QBA NEW_ROW_LOOP


