

#include "Ball.h"

std::map<std::string, Ball> AM6232ALW;

#define PULLUDEN_SHIFT (16)
#define PULLTYPESEL_SHIFT (17)
#define RXACTIVE_SHIFT (18)
#define DEBOUNCE_SHIFT (11)
#define WKUP_EN_SHIFT (29)

#define PULL_DISABLE (1 << PULLUDEN_SHIFT)
#define PULL_ENABLE (0 << PULLUDEN_SHIFT)

#define PULL_UP (1 << PULLTYPESEL_SHIFT | PULL_ENABLE)
#define PULL_DOWN (0 << PULLTYPESEL_SHIFT | PULL_ENABLE)

#define INPUT_EN (1 << RXACTIVE_SHIFT)
#define INPUT_DISABLE (0 << RXACTIVE_SHIFT)

#define PIN_OUTPUT (INPUT_DISABLE | PULL_DISABLE)
#define PIN_OUTPUT_PULLUP (INPUT_DISABLE | PULL_UP)
#define PIN_OUTPUT_PULLDOWN (INPUT_DISABLE | PULL_DOWN)
#define PIN_INPUT (INPUT_EN | PULL_DISABLE)
#define PIN_INPUT_PULLUP (INPUT_EN | PULL_UP)
#define PIN_INPUT_PULLDOWN (INPUT_EN | PULL_DOWN)

constexpr uint32_t MCU_DOMAIN = 0;
constexpr uint32_t MAIN_DOMAIN = 1;

static Ball& addBall(const std::string& n, uint32_t d, uint32_t o, bool addGPIO = true, uint32_t pru0Base = 0xFFFFFFFF, uint32_t pru1Base = 0xFFFFFFFF) {
    Ball& b = Ball::addBall(n, d, o);
    b.addMode("reset", { 0x8214007 });
    if (addGPIO) {
        b.addMode("gpio", { PIN_INPUT | 7 }).addMode("gpio_pu", { PIN_INPUT_PULLUP | 7 }).addMode("gpio_pd", { PIN_INPUT_PULLDOWN | 7 }).addMode("gpio_out", { PIN_OUTPUT | 7 });
    }
    if (pru0Base < 0xFF) {
        b.addMode("pruout", { PIN_OUTPUT | pru1Base }).addMode("pruin", { PIN_INPUT | (pru1Base + 1) }).addMode("pru0out", { PIN_OUTPUT | pru1Base }).addMode("pru0in", { PIN_INPUT | (pru1Base + 1) });
    }
    if (pru1Base < 0xFF) {
        if (pru0Base >= 0xFF) {
            b.addMode("pruout", { PIN_OUTPUT | pru1Base }).addMode("pruin", { PIN_INPUT | (pru1Base + 1) });
        }
        b.addMode("pru1out", { PIN_OUTPUT | pru1Base }).addMode("pru1in", { PIN_INPUT | (pru1Base + 1) });
    }
    return b;
}

void InitAM6232Balls() {
    addBall("A18", MAIN_DOMAIN, 0x1F0, true);
    addBall("L23", MAIN_DOMAIN, 0x084);
    addBall("P25", MAIN_DOMAIN, 0x07C);
    addBall("M22", MAIN_DOMAIN, 0x0A4, true, 4);
    addBall("L24", MAIN_DOMAIN, 0x088);
    addBall("L25", MAIN_DOMAIN, 0x08C);
    addBall("K25", MAIN_DOMAIN, 0x0A0);
    addBall("M25", MAIN_DOMAIN, 0x03C);
    addBall("N23", MAIN_DOMAIN, 0x040);
    addBall("N24", MAIN_DOMAIN, 0x044);
    addBall("N25", MAIN_DOMAIN, 0x048);
    addBall("P24", MAIN_DOMAIN, 0x04C);
    addBall("P22", MAIN_DOMAIN, 0x050);
    addBall("P21", MAIN_DOMAIN, 0x054);
    addBall("R23", MAIN_DOMAIN, 0x058);
    addBall("R24", MAIN_DOMAIN, 0x05C);
    addBall("R25", MAIN_DOMAIN, 0x060);
    addBall("T25", MAIN_DOMAIN, 0x064);
    addBall("R21", MAIN_DOMAIN, 0x068);
    addBall("T22", MAIN_DOMAIN, 0x06C);
    addBall("T24", MAIN_DOMAIN, 0x070);
    addBall("U25", MAIN_DOMAIN, 0x074);
    addBall("U24", MAIN_DOMAIN, 0x078);
    addBall("M24", MAIN_DOMAIN, 0x090);
    addBall("N20", MAIN_DOMAIN, 0x094, true, 4);
    addBall("M21", MAIN_DOMAIN, 0x0A8);
    addBall("L21", MAIN_DOMAIN, 0x0AC);
    addBall("K22", MAIN_DOMAIN, 0x0B0, true, 4).addMode("uart4_rx", { PIN_INPUT | 3 }).addMode("i2c", { PIN_INPUT_PULLUP | 1 });
    addBall("K24", MAIN_DOMAIN, 0x0B4, true).addMode("uart4_tx", { PIN_OUTPUT | 3 }).addMode("i2c", { PIN_INPUT_PULLUP | 1 });
    addBall("U23", MAIN_DOMAIN, 0x098);
    addBall("V25", MAIN_DOMAIN, 0x09C);
    addBall("B16", MAIN_DOMAIN, 0x1E0);
    addBall("A16", MAIN_DOMAIN, 0x1E4);
    addBall("B17", MAIN_DOMAIN, 0x1E8, true).addMode("uart1_rx", { PIN_INPUT | 1 });
    addBall("A17", MAIN_DOMAIN, 0x1EC, true).addMode("uart1_tx", { PIN_OUTPUT | 1 });
    addBall("E15", MAIN_DOMAIN, 0x1DC, true).addMode("uart5_tx", { PIN_OUTPUT | 1 }).addMode("can0_rx", { PIN_INPUT | 0 });
    addBall("C15", MAIN_DOMAIN, 0x1D8, true).addMode("uart5_rx", { PIN_INPUT | 1 }).addMode("can0_tx", { PIN_OUTPUT | 0 });
    addBall("A20", MAIN_DOMAIN, 0x1B0, true).addMode("uart1_tx", { PIN_OUTPUT | 2 });
    addBall("B20", MAIN_DOMAIN, 0x1A4, true);
    addBall("E19", MAIN_DOMAIN, 0x1AC, true).addMode("uart1_rx", { PIN_INPUT | 2 });
    addBall("D20", MAIN_DOMAIN, 0x1A8, true);
    addBall("E18", MAIN_DOMAIN, 0x1A0, true);
    addBall("B18", MAIN_DOMAIN, 0x19C, true);
    addBall("A19", MAIN_DOMAIN, 0x198, true).addMode("uart6_tx", { PIN_OUTPUT | 3 });
    addBall("B19", MAIN_DOMAIN, 0x194, true).addMode("uart6_rx", { PIN_INPUT | 3 });
    addBall("D1", MCU_DOMAIN, 0x060);
    addBall("A8", MCU_DOMAIN, 0x044);
    addBall("D10", MCU_DOMAIN, 0x048);
    addBall("B3", MCU_DOMAIN, 0x038, true);
    addBall("D6", MCU_DOMAIN, 0x034, true);
    addBall("D4", MCU_DOMAIN, 0x040);
    addBall("E5", MCU_DOMAIN, 0x03C);
    addBall("B5", MCU_DOMAIN, 0x014);
    addBall("A5", MCU_DOMAIN, 0x018);
    addBall("AD24", MAIN_DOMAIN, 0x160, true);
    addBall("AB22", MAIN_DOMAIN, 0x15C, true);
    addBall("AD23", MAIN_DOMAIN, 0x180, true, 3);
    addBall("AD22", MAIN_DOMAIN, 0x17C, true, 3);
    addBall("AE21", MAIN_DOMAIN, 0x168, true, 3);
    addBall("AA19", MAIN_DOMAIN, 0x164, true);
    addBall("AE23", MAIN_DOMAIN, 0x184, true, 3);
    addBall("AB20", MAIN_DOMAIN, 0x188, true, 3);
    addBall("AC21", MAIN_DOMAIN, 0x18C, true, 3);
    addBall("AE22", MAIN_DOMAIN, 0x190, true, 3);
    addBall("Y18", MAIN_DOMAIN, 0x16C, true, 0xFFFF, 3);
    addBall("AA18", MAIN_DOMAIN, 0x170, true, 0xFFFF, 3);
    addBall("AD21", MAIN_DOMAIN, 0x174, true, 0xFFFF, 3);
    addBall("AC20", MAIN_DOMAIN, 0x178, true, 0xFFFF, 3);
    addBall("A14", MAIN_DOMAIN, 0x1BC, true);
    addBall("A13", MAIN_DOMAIN, 0x1B4, true);
    addBall("C13", MAIN_DOMAIN, 0x1B8, true);
    addBall("B13", MAIN_DOMAIN, 0x1C0, true);
    addBall("B14", MAIN_DOMAIN, 0x1C4, true);
    addBall("A15", MAIN_DOMAIN, 0x1D0, true).addMode("uart2_rx", { PIN_INPUT | 3 }).addMode("i2c", { PIN_INPUT_PULLUP | 2 });
    addBall("B15", MAIN_DOMAIN, 0x1D4, true).addMode("uart3_tx", { PIN_OUTPUT | 3 }).addMode("i2c", { PIN_INPUT_PULLUP | 2 });
    addBall("D14", MAIN_DOMAIN, 0x1C8, true).addMode("uart0_rx", { PIN_INPUT | 0 });
    addBall("E14", MAIN_DOMAIN, 0x1CC, true).addMode("uart0_tx", { PIN_OUTPUT | 0 });
    addBall("F18", MAIN_DOMAIN, 0x258, true);
    addBall("Y20", MAIN_DOMAIN, 0x0FC, true, 5, 2);
    addBall("AB24", MAIN_DOMAIN, 0x0F8, true, 0, 2);
    addBall("AC24", MAIN_DOMAIN, 0x104, true, 5, 2);
    addBall("AC25", MAIN_DOMAIN, 0x100, true, 5, 2);
    addBall("U22", MAIN_DOMAIN, 0x0B8, true, 5, 2).addMode("uart2_rx", { PIN_INPUT | 4 });
    addBall("V24", MAIN_DOMAIN, 0x0BC, true, 5, 2).addMode("uart2_tx", { PIN_OUTPUT | 4 });
    addBall("W25", MAIN_DOMAIN, 0xC0, true, 5, 2).addMode("uart3_rx", { PIN_INPUT | 4 });
    addBall("W24", MAIN_DOMAIN, 0x0C4, true, 5, 2);
    addBall("Y25", MAIN_DOMAIN, 0x0C8, true, 5, 2).addMode("uart4_rx", { PIN_INPUT | 4 });
    addBall("Y24", MAIN_DOMAIN, 0x0CC, true, 5, 2).addMode("uart4_tx", { PIN_OUTPUT | 4 });
    addBall("Y23", MAIN_DOMAIN, 0x0D0, true, 5, 2).addMode("uart5_rx", { PIN_INPUT | 4 });
    addBall("AA25", MAIN_DOMAIN, 0x0D4, true, 5, 2).addMode("uart5_tx", { PIN_OUTPUT | 4 });
    addBall("V21", MAIN_DOMAIN, 0x0D8, true, 5, 2).addMode("uart6_rx", { PIN_INPUT | 4 });
    addBall("W21", MAIN_DOMAIN, 0x0DC, true, 5, 2);
    addBall("V20", MAIN_DOMAIN, 0x0E0, true, 5, 2);
    addBall("AA23", MAIN_DOMAIN, 0x0E4, true, 5, 2);
    addBall("AB25", MAIN_DOMAIN, 0x0E8, true, 5, 2);
    addBall("AA24", MAIN_DOMAIN, 0x0EC, true, 5, 2);
    addBall("Y22", MAIN_DOMAIN, 0x0F0, true, 5, 2);
    addBall("AA21", MAIN_DOMAIN, 0x0F4, true, 5, 2);
    addBall("AE18", MAIN_DOMAIN, 0x13C, true);
    addBall("AD18", MAIN_DOMAIN, 0x140, true);

    addBall("B9", MCU_DOMAIN, 0x04C);
    addBall("A9", MCU_DOMAIN, 0x050);
}