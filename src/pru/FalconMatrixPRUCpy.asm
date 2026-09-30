#include "ResourceTable.asm"


#ifndef RUNNING_ON_PRU0
#ifndef RUNNING_ON_PRU1
#define RUNNING_ON_PRU1
#endif
#endif

; this file is always the "copy" PRU side
#define IN_CPY_PRU

#include "FalconPRUDefs.hp"
#include "FalconUtils.asm"

#ifdef SPLIT_GPIO
;*****************************************************************************
; Split output mode: prefetch the pixel data from DDR, write the GPIO banks
; this PRU owns (CPY_OWNS_GPIOx) itself, then publish the pixel's four bank
; words + sequence number to the output PRU via scratchpad bank 11
; (r17..r21).  The output PRU writes its banks + the clock and acknowledges
; through bank 10 (r15); a pixel's banks are only ever written after the
; previous pixel was clocked, so the data can never change under a pending
; clock edge, and this PRU's writes are always posted before the output
; PRU's clock write.
;
; The scratchpad banks are PACKED (transfers always start at bank offset
; 0), so each bank carries one record and every transfer moves all of it:
;   bank 10 (output PRU -> here): r14 = control word (frame base address,
;            0 to park, 0xFFFFFFF to exit), r15 = pixels-clocked sequence
;   bank 11 (here -> output PRU): r17 = published sequence, r18-r21 words
; Both sequence counters restart at 0 each frame.
;*****************************************************************************

#define ctrl_word     r14    // control: frame base / 0 / exit sentinel
#define gpio0_led_mask r2
#define gpio1_led_mask r3
#define gpio2_led_mask r4
#define gpio3_led_mask r5
#define cur_addr      r6
#define endVal        r7
#define cpy_gpio_base r8
#ifdef GPIO_CLR_FIRST
#define cpy_out_clr   r9
#define cpy_out_set   r10
#define cpy_out_clrset cpy_out_clr
#else
#define cpy_out_set   r9
#define cpy_out_clr   r10
#define cpy_out_clrset cpy_out_set
#endif
#define cpy_last0     r11    // per-bank change detection state
#define cpy_last1     r12
#define cpy_last2     r13
#define cpy_last3     r16
#define clocked_seq   r15    // output PRU's acknowledge counter
#define publish_seq   r17    // publish record: sequence + 4 bank words
#define pixel_data    r18
#define frame_addr    r26

#define CAT3(X,Y,Z) X##Y##Z
#define GPIO_MASK(X) CAT3(gpio,X,_led_mask)
#define CONFIGURE_PIN(a) SET GPIO_MASK(a##_gpio), GPIO_MASK(a##_gpio), a##_pin

#include "/tmp/PanelPinConfiguration.hp"

#if defined(OUTPUTBYROW) && defined(OUTPUTBLANKROW)
; write one owned bank; no change detection - the by-row blank row clears
; the pins behind our back so the pin state must always be rewritten
CPY_OUTPUT_GPIO .macro data, mask, gpio, last
    LDI32 cpy_gpio_base, gpio
    AND   cpy_out_set, data, mask
    XOR   cpy_out_clr, cpy_out_set, mask
    SBBO  &cpy_out_clrset, cpy_gpio_base, GPIO_SETCLRDATAOUT, 8
    .endm
#else
; write one owned bank with the same change detection / fast paths as the
; output PRU's OUTPUT_GPIO; valid because nothing else ever changes these
; pins in by-depth mode and the pin state persists across frames
CPY_OUTPUT_GPIO .macro data, mask, gpio, last
    .newblock
    QBEQ DONECPYOUT?, data, last
    MOV  last, data
    LDI32 cpy_gpio_base, gpio
    QBEQ SETALL?, data, mask
    QBEQ CLEARALL?, data, 0
        AND   cpy_out_set, data, mask
        XOR   cpy_out_clr, cpy_out_set, mask
        SBBO  &cpy_out_clrset, cpy_gpio_base, GPIO_SETCLRDATAOUT, 8
        QBA DONECPYOUT?
SETALL?:
        SBBO  &data, cpy_gpio_base, GPIO_SETDATAOUT, 4
        QBA DONECPYOUT?
CLEARALL?:
        SBBO  &mask, cpy_gpio_base, GPIO_CLRDATAOUT, 4
DONECPYOUT?:
    .endm
#endif

; write all the banks this PRU owns for one pixel
CPY_DO_BANKS .macro d0, d1, d2, d3
#ifdef CPY_OWNS_GPIO0
    CPY_OUTPUT_GPIO d0, gpio0_led_mask, GPIO0, cpy_last0
#endif
#ifdef CPY_OWNS_GPIO1
    CPY_OUTPUT_GPIO d1, gpio1_led_mask, GPIO1, cpy_last1
#endif
#ifdef CPY_OWNS_GPIO2
    CPY_OUTPUT_GPIO d2, gpio2_led_mask, GPIO2, cpy_last2
#endif
#ifdef CPY_OWNS_GPIO3
    CPY_OUTPUT_GPIO d3, gpio3_led_mask, GPIO3, cpy_last3
#endif
    .endm

; spin step shared by the per-pixel waits: watch for a control change and
; for the output PRU's acknowledge to catch up
    .sect    ".text:main"
    .clink
    .global    ||main||

||main||:
#ifdef AM33XX
    ;Enable OCP master port
    LBCO    &r0, C4, 4, 4
    CLR     r0, r0, 4
    SBCO    &r0, C4, 4, 4
#endif

    ; let the host know we have started
    LDI     r0, 1
    SBCO    &r0, C24, 0, 4

    LDI32   endVal, 0xFFFFFFF

    LDI     gpio0_led_mask, 0
    LDI     gpio1_led_mask, 0
    LDI     gpio2_led_mask, 0
    LDI     gpio3_led_mask, 0
    LDI     cpy_last0, 0
    LDI     cpy_last1, 0
    LDI     cpy_last2, 0
    LDI     cpy_last3, 0
#include "FalconMatrixConfigPins.asm"

    ; the scratchpad survives restarts - clear the control record so a
    ; stale exit sentinel or frame address from a previous run can't be
    ; acted on (the output PRU republishes the frame address every frame)
    LDI     ctrl_word, 0
    LDI     clocked_seq, 0
    XOUT    10, &ctrl_word, 8

FRAME_WAIT:
    XIN     10, &ctrl_word, 8
    QBEQ    EXIT, ctrl_word, endVal
    QBEQ    FRAME_WAIT, ctrl_word, 0

    ; new frame: align to the output PRU's freshly reset counter and
    ; invalidate any stale publish
    MOV     frame_addr, ctrl_word
    MOV     cur_addr, ctrl_word
    MOV     publish_seq, clocked_seq
    XOUT    11, &publish_seq, 4

PAIR_LOOP:
    LBBO    &pixel_data, cur_addr, 0, 32
    ADD     cur_addr, cur_addr, 32

    ; ---- first pixel ----
WAITA:
    XIN     10, &ctrl_word, 8
    QBNE    CTRL_CHANGE, ctrl_word, frame_addr
    QBEQ    DOPIXA, clocked_seq, publish_seq
    ; in lockstep the acknowledge can only be at or one behind the publish;
    ; anything else means the frame restarted while we were blind in the
    ; LBBO above (the park pulse is short and the frame address may be
    ; unchanged on a re-display) - resync from the frame base
    SUB     r0, publish_seq, 1
    QBNE    RESYNC, clocked_seq, r0
    QBA     WAITA
DOPIXA:
    CPY_DO_BANKS r18, r19, r20, r21
    ADD     publish_seq, publish_seq, 1
    XOUT    11, &publish_seq, 20

    ; ---- second pixel ----
WAITB:
    XIN     10, &ctrl_word, 8
    QBNE    CTRL_CHANGE, ctrl_word, frame_addr
    QBEQ    DOPIXB, clocked_seq, publish_seq
    SUB     r0, publish_seq, 1
    QBNE    RESYNC, clocked_seq, r0
    QBA     WAITB
DOPIXB:
    CPY_DO_BANKS r22, r23, r24, r25
    MOV     r18, r22
    MOV     r19, r23
    MOV     r20, r24
    MOV     r21, r25
    ADD     publish_seq, publish_seq, 1
    XOUT    11, &publish_seq, 20
    QBA     PAIR_LOOP

RESYNC:
    ; the output PRU restarted the frame: re-align and start over from the
    ; frame base address
    MOV     cur_addr, frame_addr
    MOV     publish_seq, clocked_seq
    XOUT    11, &publish_seq, 4
    QBA     PAIR_LOOP

CTRL_CHANGE:
    QBEQ    EXIT, ctrl_word, endVal
    QBA     FRAME_WAIT

EXIT:
    ; consume the exit sentinel: the scratchpad survives a firmware reload,
    ; and the next run starts this PRU before the output PRU
    LDI     ctrl_word, 0
    LDI     clocked_seq, 0
    XOUT    10, &ctrl_word, 8
    LDI   R31.b0, PRU_ARM_INTERRUPT+16
    halt

#else
;*****************************************************************************
;  Classic mode: pure data prefetcher.
;*****************************************************************************

#define data_addr     r1
#define endVal        r7
#define lastData      r17
#define pixel_data    r18

    .sect    ".text:main"
    .clink
    .global    ||main||

||main||:
#ifdef AM33XX
    ;Enable OCP master port
    ; clear the STANDBY_INIT bit in the SYSCFG register,
    ; otherwise the PRU will not be able to write outside the
    ; PRU memory space and to the BeagleBone pins.
    LBCO    &r0, C4, 4, 4
    CLR     r0, r0, 4
    SBCO    &r0, C4, 4, 4
#endif

    ; Configure the programmable pointer register for PRU0 by setting
    ; c28_pointer[15:0] field to 0x0120.  This will make C28 point to
    ; 0x00012000 (PRU shared RAM).
    LDI		r0, 0x00000120
    LDI32	r1, CTPPR_0 + PRU_MEMORY_OFFSET
    SBBO    &r0, r1, 0x00, 4

    ; Configure the programmable pointer register for PRU0 by setting
    ; c31_pointer[15:0] field to 0x0010.  This will make C31 point to
    ; 0x80001000 (DDR memory).
    LDI32	r0, 0x00100000
    LDI32	r1, CTPPR_1 + PRU_MEMORY_OFFSET
    SBBO    &r0, r1, 0x00, 4

    LDI     r3, 1
    SBCO    &r3, C24, 0, 4

    LDI32   endVal, 0xFFFFFFF
    LDI     data_addr, 0
    ; the scratchpad survives a firmware reload, and this PRU is started
    ; before the output PRU: clear the control word so an exit sentinel
    ; left by the previous run's output PRU cannot stop this one at once
    XOUT    10, &data_addr, 4

READ_LOOP:
    XIN 10, &data_addr, 4

    ; Wait for a non-zero address
    QBEQ READ_LOOP, data_addr, 0

    ; Command of 0xFFFFFFF is the signal to exit
    QBEQ EXIT, data_addr, endVal

    ; nothing changed, re-read
    QBEQ READ_LOOP, lastData, data_addr

    LBBO    &pixel_data, data_addr, 0, 32
    MOV     lastData, data_addr
    XOUT    11, &lastData, (32 + 4)
    QBA READ_LOOP

EXIT:
    ; consume the exit sentinel before halting (see the start-up clear)
    LDI     data_addr, 0
    XOUT    10, &data_addr, 4
    LDI   R31.b0, PRU_ARM_INTERRUPT+16
    halt                    ; Halt PRU execution
#endif

;*****************************************************************************
;                                     END
;*****************************************************************************
