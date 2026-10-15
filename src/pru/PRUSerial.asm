/* Serial transmitter on a GPIO pin.
 *
 * For a header pin that has no UART transmit on this board: the PocketBeagle 2
 * only offers receive functions on P1-10, where PocketBeagle capes put their
 * DMX/Renard line driver.  The PRU writes the pin's GPIO set/clear registers,
 * timing each bit off its own cycle counter.
 *
 * The ARM side (util/BBBPruSerial.cpp) frames each byte itself (start bit,
 * data, parity, stop bits) and queues it in a ring in this PRU's data RAM, so
 * this program only shifts out bit patterns and holds the line low or high
 * for a number of cycles (break and mark-after-break).
 *
 * Data RAM layout, shared with BBBPruSerial.cpp:
 *   0x00  command   ARM: 1 = config is valid, start; 0xFF = stop
 *   0x04  response  PRU: 0xFFFF once stopped
 *   0x08  head      ARM: index of the next ring entry it will write
 *   0x0C  tail      PRU: index of the next entry to send; an entry is only
 *                   counted once its last bit has gone out, so head == tail
 *                   means the line is idle
 *   0x10  set       address of the pin's GPIO set register
 *   0x14  clear     address of the pin's GPIO clear register
 *   0x18  mask      the pin's bit in those registers
 *   0x1C  bit       cycles per bit
 *   0x100 ring      RING_ENTRIES 32 bit entries:
 *         bit 31 set: hold the line low for bits 0-29 cycles
 *         bit 30 set: hold the line high for bits 0-29 cycles
 *         neither:    send bits 16-20 bits of bits 0-15, LSB first
 *
 * The cycle counter saturates after 2^32 cycles and the hardware then clears
 * its enable bit, after which it reads 0 forever.  So CTR_EN is set once,
 * never toggled, and the counter is zeroed (a write while enabled zeroes it)
 * before every timed entry and on every idle poll.
 */

#include "ResourceTable.asm"

#include "FalconPRUDefs.hp"
#include "FalconUtils.asm"

#define CMD_OFF      0x00
#define RESP_OFF     0x04
#define HEAD_OFF     0x08
#define TAIL_OFF     0x0C
#define CONFIG_OFF   0x10
#define RING_OFF     0x100
#define RING_ENTRIES 1024

#define tmp         r0
#define tmp2        r1
#define head        r2
#define tail        r3
#define setAddr     r4   // r4-r7 are loaded together from CONFIG_OFF
#define clrAddr     r5
#define pinMask     r6
#define bitCycles   r7
#define ctrlReg     r8
#define target      r9
#define entry       r10
#define bitCount    r11
#define zero        r12
#define cycle       r13
#define ringMask    r14
#define ringBase    r15

    .sect    ".text:main"
    .global    ||main||
||main||:
    // BBBPru::run() clears this data RAM before starting the core, so the
    // command cannot be one left from a previous run
    LDI     zero, 0
    LDI32   ctrlReg, PRU_CONTROL_REG
    LBBO    &tmp, ctrlReg, 0, 4
    SET     tmp, tmp, 3
    SBBO    &tmp, ctrlReg, 0, 4

    LDI     ringMask, RING_ENTRIES - 1
    LDI     ringBase, RING_OFF
    LDI     tail, 0

WAIT_CONFIG:
    SBBO    &zero, ctrlReg, 0xC, 4
    LBCO    &tmp, CONST_PRUDRAM, CMD_OFF, 4
    QBEQ    EXIT, tmp, 0xFF
    QBNE    WAIT_CONFIG, tmp, 1

    LBCO    &setAddr, CONST_PRUDRAM, CONFIG_OFF, 16
    // idle line is high (mark)
    SBBO    &pinMask, setAddr, 0, 4

IDLE:
    SBBO    &zero, ctrlReg, 0xC, 4
    LBCO    &tmp, CONST_PRUDRAM, CMD_OFF, 4
    QBEQ    EXIT, tmp, 0xFF
    LBCO    &head, CONST_PRUDRAM, HEAD_OFF, 4
    QBEQ    IDLE, head, tail

    LSL     tmp, tail, 2
    ADD     tmp, tmp, ringBase
    LBCO    &entry, CONST_PRUDRAM, tmp, 4
    ADD     tail, tail, 1
    AND     tail, tail, ringMask
    QBBS    HOLD_LOW, entry, 31
    QBBS    HOLD_HIGH, entry, 30

    // a framed character: bitCount bits of entry, LSB first, each driven at
    // a whole multiple of bitCycles after the first so timing errors do not
    // add up across the character
    LSR     bitCount, entry, 16
    AND     bitCount, bitCount, 0x1F
    QBEQ    ENTRY_DONE, bitCount, 0
    MOV     target, bitCycles
    SBBO    &zero, ctrlReg, 0xC, 4
NEXT_BIT:
    QBBC    BIT_LOW, entry, 0
    SBBO    &pinMask, setAddr, 0, 4
    QBA     BIT_WAIT
BIT_LOW:
    SBBO    &pinMask, clrAddr, 0, 4
BIT_WAIT:
    LBBO    &cycle, ctrlReg, 0xC, 4
    QBGT    BIT_WAIT, cycle, target
    ADD     target, target, bitCycles
    LSR     entry, entry, 1
    SUB     bitCount, bitCount, 1
    QBNE    NEXT_BIT, bitCount, 0
    QBA     ENTRY_DONE

HOLD_LOW:
    SBBO    &pinMask, clrAddr, 0, 4
    QBA     HOLD
HOLD_HIGH:
    SBBO    &pinMask, setAddr, 0, 4
HOLD:
    LDI32   tmp2, 0x3FFFFFFF
    AND     target, entry, tmp2
    SBBO    &zero, ctrlReg, 0xC, 4
HOLD_WAIT:
    LBBO    &cycle, ctrlReg, 0xC, 4
    QBGT    HOLD_WAIT, cycle, target

ENTRY_DONE:
    SBCO    &tail, CONST_PRUDRAM, TAIL_OFF, 4
    QBA     IDLE

EXIT:
    LDI32   tmp, 0xFFFF
    SBCO    &tmp, CONST_PRUDRAM, RESP_OFF, 4
    LDI     R31.b0, PRU_ARM_INTERRUPT+16
    HALT
