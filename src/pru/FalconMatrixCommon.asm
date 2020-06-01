
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
    LBBO &out_clr, gpio_base_cache, GPIO_DATAOUT, 4
    .endm

CLOCK_HI .macro
    LDI32or16 out_set, 1 << gpio_clock
    SBBO &out_set, gpio_base_cache, GPIO_SETDATAOUT, 4
    .endm

CLOCK_LO .macro
    // we normally can lower the clock line at the same time as outputing the
    // gpio data if we're outputting data on this GPIO
#ifdef NO_CONTROLS_WITH_DATA
    LDI32or16 out_clr, 1 << gpio_clock
    SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 4
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



OUTPUT_ROW_ADDRESS .macro
#ifdef gpio_sel0
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
    SBBO &out_clr, gpio_base_cache, GPIO_CLRDATAOUT, 8 // set both
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


#endif
