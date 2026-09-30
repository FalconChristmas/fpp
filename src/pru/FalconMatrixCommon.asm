
#ifndef __octoscoller_common__
#define __octoscoller_common__

#ifdef E_SCAN_LINE
#ifdef gpio_sel0
#define GPIO_SEL_MASK (0\
|(1<<gpio_sel0)\
|(1<<gpio_sel1)\
|(1<<gpio_sel2)\
|(1<<gpio_sel3)\
|(1<<gpio_sel4)\
)
#endif
#ifdef pru_sel0
#define PRU_SEL_MASK (0\
|(1<<pru_sel0)\
|(1<<pru_sel1)\
|(1<<pru_sel2)\
|(1<<pru_sel3)\
|(1<<pru_sel4)\
)
#endif
#else
#ifdef gpio_sel0
#define GPIO_SEL_MASK (0\
|(1<<gpio_sel0)\
|(1<<gpio_sel1)\
|(1<<gpio_sel2)\
|(1<<gpio_sel3)\
)
#endif
#ifdef pru_sel0
#define PRU_SEL_MASK (0\
|(1<<pru_sel0)\
|(1<<pru_sel1)\
|(1<<pru_sel2)\
|(1<<pru_sel3)\
)
#endif
#endif


LDI32or16 .macro a, b
    .if b <= 0xFFFF
    LDI a, b
    .else
    LDI32 a, b
    .endif
    .endm

READ_TO_FLUSH .macro
    //read the base line to make sure all SET/CLR are
    //flushed and out
    LBBO &out_clr, gpio_base_cache, GPIO_DATAIN, 4
    .endm

CLOCK_HI .macro
#ifdef gpio_clock
    LDI32or16 out_set, 1 << gpio_clock
    SBBO &out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
#ifndef AM33XX    
    // On the AM62x, we need to flush the clock line
    // and lower it immediately or we can get some 
    // write combining and the pulses may be too
    // short to register
    READ_TO_FLUSH
    LDI32or16 out_clr, 1 << gpio_clock
    SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
#endif    
#else
    SET r30, r30, pru_clock
#endif      
    .endm

CLOCK_LO .macro
#ifdef gpio_clock
#ifdef AM33XX
    // we normally can lower the clock line at the same time as outputing the
    // gpio data if we're outputting data on this GPIO
#ifdef NO_CONTROLS_WITH_DATA
    LDI32or16 out_clr, 1 << gpio_clock
    SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
#endif
#endif
#else
    CLR r30, r30, pru_clock
#endif
    .endm


LATCH_HI .macro
    // LATCH HI NEEDS to be completely independent of
    // all other GPIO calls or ghosting occurs.  We'll
    // flush before and after the latch
    READ_TO_FLUSH
#ifdef gpio_latch
    LDI32or16 out_set, 1 << gpio_latch
    SBBO &out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
    READ_TO_FLUSH
#else
    SET r30, r30, pru_latch
#endif
    .endm

LATCH_LO .macro
   // we can lower the latch line at the same time as outputing the
   // gpio data so this doesn't need to be implemented
#ifdef gpio_latch
#ifdef NO_CONTROLS_WITH_DATA
   LDI32or16 out_clr, 1 << gpio_latch
   SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
#endif
#else
    CLR r30, r30, pru_latch
#endif
    .endm

DISPLAY_OFF .macro
#if defined(USING_PWM)
    LDI32 out_set, oe_pwm_address
    LDI out_clr, 0
    SBBO &out_clr, out_set, 0x12 + oe_pwm_output * 2, 2
#else
#ifdef gpio_oe
    LDI32or16 out_set, 1 << gpio_oe
    SBBO &out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
#else
    SET r30, r30, pru_oe
#endif
#endif
    .endm

DISPLAY_ON .macro
#if defined(USING_PWM)
    LDI32 out_clr, oe_pwm_address
    //set the "on time"
    MOV out_set, sleep_counter
    SBBO &out_set, out_clr, 0x12 + oe_pwm_output * 2, 2
    //reset the timer
    LDI out_set, 0
    SBBO &out_set, out_clr, 0x8, 2
#else
#ifdef gpio_oe
    LDI32or16 out_clr, 1 << gpio_oe
    SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
#else
    CLR r30, r30, pru_oe
#endif
#endif
    .endm



#ifdef ADDRESSING_ABC_SHIFT
#ifndef ROW_CHAIN
#define ROW_CHAIN ROWS
#endif
// Row select for panels whose row drivers are shift registers clocked on A
// with data on C (rpi-rgb-led-matrix row address type 3).  Every row change
// clocks the whole chain - ROW_CHAIN bits, the panel height, since panels
// daisy chain one driver per band of ROWS rows - with the row's bit repeated
// every ROWS bits, one clock edge per bit.  B is held low: the panel may use
// it for something else.  The display is off while this runs.
OUTPUT_ROW_ADDRESS_ABC_SHIFT .macro
    .newblock
    // Let the column drivers go dark first: the very first clock walks the
    // row bit onto the next row, and the tail of the old data would show
    // there as a faint copy of each pixel.
    LDI  tmp_reg1, NS2CLK(1000) / 2
PREDLY?:
    SUB  tmp_reg1, tmp_reg1, 1
    QBNE PREDLY?, tmp_reg1, 0
    LDI  tmp_reg4.b0, ROWS - 1
    SUB  tmp_reg4.b0, tmp_reg4.b0, row       // active position within a band
    LDI  tmp_reg4.b1, 0                      // position within the band
    LDI  tmp_reg4.w2, 0                      // bits clocked
#ifdef gpio_sel0
SHIFT?:
    // clock low, B low and the data bit on C, in one clear/set write
    LDI32 out_clr, (1 << gpio_sel0) | (1 << gpio_sel1)
    LDI  out_set, 0
    QBEQ ACTIVE?, tmp_reg4.b1, tmp_reg4.b0
    SET  out_clr, out_clr, gpio_sel2
    QBA  WRITE?
ACTIVE?:
    SET  out_set, out_set, gpio_sel2
WRITE?:
    SBBO &out_clrset, gpio_base_cache, GPIO_SETCLRDATAOUT, 8
    LDI  tmp_reg1, NS2CLK(80) / 2
DLY1?:
    SUB  tmp_reg1, tmp_reg1, 1
    QBNE DLY1?, tmp_reg1, 0
    LDI32 out_set, (1 << gpio_sel0)
    LDI  out_clr, 0
    SBBO &out_clrset, gpio_base_cache, GPIO_SETCLRDATAOUT, 8
#else
    CLR  r30, r30, pru_sel1
SHIFT?:
    CLR  r30, r30, pru_sel0
    QBEQ ACTIVE?, tmp_reg4.b1, tmp_reg4.b0
    CLR  r30, r30, pru_sel2
    QBA  CLOCK?
ACTIVE?:
    SET  r30, r30, pru_sel2
CLOCK?:
    LDI  tmp_reg1, NS2CLK(80) / 2
DLY1?:
    SUB  tmp_reg1, tmp_reg1, 1
    QBNE DLY1?, tmp_reg1, 0
    SET  r30, r30, pru_sel0
#endif
    LDI  tmp_reg1, NS2CLK(80) / 2
DLY2?:
    SUB  tmp_reg1, tmp_reg1, 1
    QBNE DLY2?, tmp_reg1, 0
    ADD  tmp_reg4.b1, tmp_reg4.b1, 1
    QBNE NOWRAP?, tmp_reg4.b1, ROWS
    LDI  tmp_reg4.b1, 0
NOWRAP?:
    ADD  tmp_reg4.w2, tmp_reg4.w2, 1
    QBNE SHIFT?, tmp_reg4.w2, ROW_CHAIN
    // Leave the clock low - exactly one edge per bit, no trailing pulse -
    // and then, after the last bit's hold time, the data line low too.  The
    // last bit is the active one for row 0, and a data line left high for
    // that row's whole display time made it flicker.
#ifdef gpio_sel0
    LDI  out_set, 0
    LDI32 out_clr, (1 << gpio_sel0)
    SBBO &out_clrset, gpio_base_cache, GPIO_SETCLRDATAOUT, 8
#else
    CLR  r30, r30, pru_sel0
#endif
    LDI  tmp_reg1, NS2CLK(80) / 2
DLY3?:
    SUB  tmp_reg1, tmp_reg1, 1
    QBNE DLY3?, tmp_reg1, 0
#ifdef gpio_sel0
    LDI32 out_clr, (1 << gpio_sel2)
    SBBO &out_clrset, gpio_base_cache, GPIO_SETCLRDATAOUT, 8
#else
    CLR  r30, r30, pru_sel2
#endif
    .endm
#endif

OUTPUT_ROW_ADDRESS .macro
#ifdef ADDRESSING_ABC_SHIFT
    OUTPUT_ROW_ADDRESS_ABC_SHIFT
#elif defined(gpio_sel0)
    // set address; select pins in gpio1 are sequential
    // xor with the select bit mask to set which ones should
#ifdef ADDRESSING_AB
    LDI32 out_set, GPIO_SEL_MASK
    MOV out_clr, row
    ADD out_clr, out_clr, gpio_sel0
    CLR out_set, out_set, out_clr
#else
    MOV out_set, row
    LSL out_set, out_set, gpio_sel0
#endif
    LDI32 out_clr, GPIO_SEL_MASK
    AND out_set, out_set, out_clr // ensure no extra bits
    XOR out_clr, out_set, out_clr // complement the bits into clr
    SBBO &out_clrset, gpio_base_cache, GPIO_SETCLRDATAOUT, 8 // set both
#else
#ifdef ADDRESSING_AB
    MOV out_clr, row
    ADD out_clr, out_clr, pru_sel0

    OR r30, r30, PRU_SEL_MASK   //all the sel's high
#ifdef pru_sel4
    CLR r30, r30, pru_sel4           //keep E line low
#endif
    CLR r30, r30, out_clr            //lower the line we need
#else
    LSL out_set, row, pru_sel0
    LDI32 out_clr, PRU_SEL_MASK
    NOT out_clr, out_clr
    AND r30, r30, out_clr // clear the address bits
    OR  r30, r30, out_set // set the address bits
#endif
#endif
    .endm


ADJUST_SETTINGS .macro
    LDI32 gpio_base_cache, CONTROLS_GPIO_BASE

#ifndef NO_CONTROLS_WITH_DATA
#ifdef gpio_clock
    //we're going to clear the clock along with the pixel data
    SET  gpio_controls_led_mask, gpio_controls_led_mask, gpio_clock
#endif
#ifdef gpio_latch
    //also clear the latch along with the pixel data
    SET  gpio_controls_led_mask, gpio_controls_led_mask, gpio_latch
#endif
#endif
    .endm



READ_DATA .macro
#ifdef SINGLEPRU
    CHECK_FOR_DISPLAY_OFF
    LBBO    &pixel_data, data_addr, 0, 32
    ADD     data_addr, data_addr, 32
#else
REREAD:
        ; we do this by transfering the data_addr to the other PRU via XOUT
        ; which will LBBO the data and then transfer it back via XIN
    CHECK_FOR_DISPLAY_OFF
    XIN 11, &data_from_other_pru, (32 + 4)
    QBNE REREAD, data_from_other_pru, data_addr
    ADD data_addr, data_addr, 32
    ; XOUT the new data_addr so the other RPU can start working
    ; on loading it while we process this data
    XOUT 10, &data_addr, 4
#endif
    .endm


#ifdef SPLIT_GPIO
; Split output mode: the copy PRU prefetches the pixel data, writes the GPIO
; banks it owns (CPY_OWNS_GPIOx) itself, then publishes the pixel's four
; bank words + a sequence number through scratchpad bank 11 (r17..r21).
; This PRU waits for the publish, writes its own banks + the clock, and
; acknowledges through bank 10 - so the copy PRU's data writes always land
; before this PRU's clock edge, and never change under a pending clock.
;
; NOTE: the scratchpad banks are PACKED - a transfer always starts at bank
; offset 0 no matter which register it starts from - so each bank carries
; exactly one record and every transfer moves the whole record:
;   bank 10 (this PRU -> copy): r14 = control (frame base address, 0 to
;            park, 0xFFFFFFF to exit), r15 = pixels-clocked sequence
;   bank 11 (copy -> this PRU): r17 = published sequence, r18-r21 = the
;            pixel's four GPIO bank words
; Both sequence counters restart at 0 each frame, so a stale publish from
; the tail of the previous frame can never match the first expected pixel.
#define split_ctrl        r14
#define split_pixel_seq   r15

; announce a new frame: reset the pixel sequence and publish the frame
; base address (data_addr must already be loaded)
SPLIT_FRAME_START .macro
    MOV  split_ctrl, data_addr
    LDI  split_pixel_seq, 0
    XOUT 10, &split_ctrl, 8
    .endm

; park the copy PRU between frames
SPLIT_FRAME_END .macro
    LDI  split_ctrl, 0
    XOUT 10, &split_ctrl, 8
    .endm

; tell the copy PRU to exit
SPLIT_EXIT .macro
    LDI32 split_ctrl, 0xFFFFFFF
    XOUT 10, &split_ctrl, 8
    .endm

; wait for the copy PRU to publish the next pixel (and its own banks to be
; written); leaves the four bank words in r18-r21
WAIT_SPLIT_DATA .macro
    .newblock
    ADD  split_pixel_seq, split_pixel_seq, 1
WAITPUB?:
    XIN  11, &data_from_other_pru, 20
    QBNE WAITPUB?, data_from_other_pru, split_pixel_seq
    .endm

; release the copy PRU to write its banks for the next pixel
ACK_SPLIT .macro
    XOUT 10, &split_ctrl, 8
    .endm

; drive the copy-owned banks' data pins low (used for the blank row where
; the copy PRU is parked and this PRU clocks out zeros)
CLEAR_CPY_BANKS .macro
#ifdef CPY_OWNS_GPIO0
    LDI32 gpio_base, GPIO0
    MOV   out_clr, gpio0_led_mask
    SBBO  &out_clr, gpio_base, GPIO_CLRDATAOUT, 4
#endif
#ifdef CPY_OWNS_GPIO1
    LDI32 gpio_base, GPIO1
    MOV   out_clr, gpio1_led_mask
    SBBO  &out_clr, gpio_base, GPIO_CLRDATAOUT, 4
#endif
#ifdef CPY_OWNS_GPIO2
    LDI32 gpio_base, GPIO2
    MOV   out_clr, gpio2_led_mask
    SBBO  &out_clr, gpio_base, GPIO_CLRDATAOUT, 4
#endif
#ifdef CPY_OWNS_GPIO3
    LDI32 gpio_base, GPIO3
    MOV   out_clr, gpio3_led_mask
    SBBO  &out_clr, gpio_base, GPIO_CLRDATAOUT, 4
#endif
    .endm
#endif

#endif
