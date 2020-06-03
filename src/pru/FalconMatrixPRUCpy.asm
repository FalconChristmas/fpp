	.include ResourceTable.asm

;*****************************************************************************
;  Define storage registers
;*****************************************************************************

data_addr             .set    r1
endVal                .set    r7
lastData              .set    r17
pixel_data            .set    r18     ; and the next 8 registers


; RUNNING_ON_PRU        .set    1

  .if RUNNING_ON_PRU
PRU_MEMORY_OFFSET     .set    0x2000
PRU_CONTROL_REG       .set    0x00024000
PRU_ARM_INTERRUPT     .set    20
  .else
PRU_MEMORY_OFFSET     .set    0x0000
PRU_CONTROL_REG       .set    0x00022000
PRU_ARM_INTERRUPT     .set    19
  .endif

; Address for the Constant table Programmable Pointer Register 0(CTPPR_0)
CTPPR_0         .set    0x22028
; Address for the Constant table Programmable Pointer Register 1(CTPPR_1)
CTPPR_1         .set    0x2202C


;*****************************************************************************
;                                  Main Loop
;*****************************************************************************
    .sect    ".text:main"
    .clink
    .global    ||main||

||main||:
    ;Enable OCP master port
    ; clear the STANDBY_INIT bit in the SYSCFG register,
    ; otherwise the PRU will not be able to write outside the
    ; PRU memory space and to the BeagleBone's pins.
    LBCO    &r0, C4, 4, 4
    CLR     r0, r0, 4
    SBCO    &r0, C4, 4, 4

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
    ZERO    &lastData, (32+4)

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
    QBA     READ_LOOP

EXIT:
    LDI   R31.b0, PRU_ARM_INTERRUPT+16
    halt                    ; Halt PRU execution

;*****************************************************************************
;                                     END
;*****************************************************************************
