

#include "Ball.h"

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

constexpr uint32_t MAIN_DOMAIN = 0;

#define PULL_DISABLE (1 << 3)
#define PULL_UP (1 << 4)
#define INPUT_EN (1 << 5)
#define SLEWCTRL_SLOW (1 << 6)
#define SLEWCTRL_FAST 0

#define PIN_OUTPUT (PULL_DISABLE)
#define PIN_OUTPUT_PULLUP (PULL_UP)
#define PIN_OUTPUT_PULLDOWN 0
#define PIN_INPUT (INPUT_EN | PULL_DISABLE)
#define PIN_INPUT_PULLUP (INPUT_EN | PULL_UP)
#define PIN_INPUT_PULLDOWN (INPUT_EN)

static Ball& addBall(const std::string& n, uint32_t d, uint32_t o, int32_t pruOut = -1, int32_t pruIn = -1, bool addGPIO = true) {
    Ball& b = Ball::addBall(n, d, o);
    if (addGPIO) {
        b.addMode("gpio", { PIN_INPUT | 7 }).addMode("gpio_pu", { PIN_INPUT_PULLUP | 7 }).addMode("gpio_pd", { PIN_INPUT_PULLDOWN | 7 }).addMode("gpio_out", { PIN_OUTPUT | 7 });
    }
    if (pruIn >= 0) {
        b.addMode("pruin", { PIN_INPUT | static_cast<uint32_t>(pruIn) });
        b.addMode("pruin_pu", { PIN_INPUT_PULLUP | static_cast<uint32_t>(pruIn) });
        b.addMode("pruin_pd", { PIN_INPUT_PULLDOWN | static_cast<uint32_t>(pruIn) });
    }
    if (pruOut >= 0) {
        b.addMode("pruout", { PIN_OUTPUT | static_cast<uint32_t>(pruOut) });
        b.addMode("pruout_pu", { PIN_OUTPUT_PULLUP | static_cast<uint32_t>(pruOut) });
        b.addMode("pruout_pd", { PIN_OUTPUT_PULLDOWN | static_cast<uint32_t>(pruOut) });
    }
    return b;
}

void InitAM335xBalls() {
    constexpr uint32_t MEMLOCATIONS[] = { 0x44e10000, 0 };
    constexpr uint32_t MAX_OFFSET = 0x0B00;
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    int d = 0;

    while (MEMLOCATIONS[d]) {
        void* v = (void*)MEMLOCATIONS[d];
        uint8_t* gpio_map = (uint8_t*)mmap(
            v,                      /* Any address in our space will do */
            MAX_OFFSET,             /* Map length */
            PROT_READ | PROT_WRITE, /* Enable reading & writing */
            MAP_SHARED,             /* Shared with other processes */
            mem_fd,                 /* File to map */
            MEMLOCATIONS[d]         /* Offset to GPIO peripheral */
        );
        Ball::setDomainAddress(d, gpio_map);
        ++d;
        printf(" Mapped domain %d at %p\n", d - 1, gpio_map);
    }
    close(mem_fd);

    addBall("GPMC_AD0", MAIN_DOMAIN, 0x800);
    addBall("GPMC_AD1", MAIN_DOMAIN, 0x804);
    addBall("GPMC_AD2", MAIN_DOMAIN, 0x808);
    addBall("GPMC_AD3", MAIN_DOMAIN, 0x80c);
    addBall("GPMC_AD4", MAIN_DOMAIN, 0x810);
    addBall("GPMC_AD5", MAIN_DOMAIN, 0x814);
    addBall("GPMC_AD6", MAIN_DOMAIN, 0x818);
    addBall("GPMC_AD7", MAIN_DOMAIN, 0x81c);
    addBall("GPMC_AD8", MAIN_DOMAIN, 0x820).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 4 }); // U10
    addBall("GPMC_AD9", MAIN_DOMAIN, 0x824).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 4 });
    addBall("GPMC_AD10", MAIN_DOMAIN, 0x828);
    addBall("GPMC_AD11", MAIN_DOMAIN, 0x82c);
    addBall("GPMC_AD12", MAIN_DOMAIN, 0x830, 6, -1);
    addBall("GPMC_AD13", MAIN_DOMAIN, 0x834, 6, -1);
    addBall("GPMC_AD14", MAIN_DOMAIN, 0x838, -1, 6);
    addBall("GPMC_AD15", MAIN_DOMAIN, 0x83c, -1, 6);
    addBall("GPMC_A0", MAIN_DOMAIN, 0x840);
    addBall("GPMC_A1", MAIN_DOMAIN, 0x844);
    addBall("GPMC_A2", MAIN_DOMAIN, 0x848).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 6 });
    addBall("GPMC_A3", MAIN_DOMAIN, 0x84c).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 6 });
    addBall("GPMC_A4", MAIN_DOMAIN, 0x850);
    addBall("GPMC_A5", MAIN_DOMAIN, 0x854);
    addBall("GPMC_A6", MAIN_DOMAIN, 0x858);
    addBall("GPMC_A7", MAIN_DOMAIN, 0x85c);
    addBall("GPMC_A8", MAIN_DOMAIN, 0x860);
    addBall("GPMC_A9", MAIN_DOMAIN, 0x864);
    addBall("GPMC_A10", MAIN_DOMAIN, 0x868);
    addBall("GPMC_A11", MAIN_DOMAIN, 0x86c);
    addBall("GPMC_WAIT0", MAIN_DOMAIN, 0x870).addMode("uart", { PIN_INPUT_PULLUP | 6 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 6 }).addMode("ttyS4-rx", { PIN_INPUT_PULLUP | 6 });
    addBall("GPMC_WPN", MAIN_DOMAIN, 0x874).addMode("uart", { PIN_OUTPUT_PULLDOWN | 6 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 6 }).addMode("ttyS4-tx", { PIN_OUTPUT_PULLDOWN | 6 });
    addBall("GPMC_BEN1", MAIN_DOMAIN, 0x878);
    addBall("GPMC_CSN0", MAIN_DOMAIN, 0x87c);
    addBall("GPMC_CSN1", MAIN_DOMAIN, 0x880);
    addBall("GPMC_CSN2", MAIN_DOMAIN, 0x884);
    addBall("GPMC_CSN3", MAIN_DOMAIN, 0x888);
    addBall("GPMC_CLK", MAIN_DOMAIN, 0x88c);
    addBall("GPMC_ADVN_ALE", MAIN_DOMAIN, 0x890);
    addBall("GPMC_OEN_REN", MAIN_DOMAIN, 0x894);
    addBall("GPMC_WEN", MAIN_DOMAIN, 0x898);
    addBall("GPMC_BEN0_CLE", MAIN_DOMAIN, 0x89c);
    addBall("LCD_DATA0", MAIN_DOMAIN, 0x8a0, 5, 6);
    addBall("LCD_DATA1", MAIN_DOMAIN, 0x8a4, 5, 6);
    addBall("LCD_DATA2", MAIN_DOMAIN, 0x8a8, 5, 6);
    addBall("LCD_DATA3", MAIN_DOMAIN, 0x8ac, 5, 6);
    addBall("LCD_DATA4", MAIN_DOMAIN, 0x8b0, 5, 6);
    addBall("LCD_DATA5", MAIN_DOMAIN, 0x8b4, 5, 6);
    addBall("LCD_DATA6", MAIN_DOMAIN, 0x8b8, 5, 6);
    addBall("LCD_DATA7", MAIN_DOMAIN, 0x8bc, 5, 6);
    addBall("LCD_DATA8", MAIN_DOMAIN, 0x8c0).addMode("uart", { PIN_OUTPUT_PULLDOWN | 4 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 4 }).addMode("ttyS5-tx", { PIN_OUTPUT_PULLDOWN | 4 });
    addBall("LCD_DATA9", MAIN_DOMAIN, 0x8c4).addMode("uart", { PIN_INPUT_PULLUP | 4 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 4 }).addMode("ttyS5-rx", { PIN_INPUT_PULLUP | 4 });
    addBall("LCD_DATA10", MAIN_DOMAIN, 0x8c8).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 2 });
    addBall("LCD_DATA11", MAIN_DOMAIN, 0x8cc).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 2 });
    addBall("LCD_DATA12", MAIN_DOMAIN, 0x8d0);
    addBall("LCD_DATA13", MAIN_DOMAIN, 0x8d4);
    addBall("LCD_DATA14", MAIN_DOMAIN, 0x8d8).addMode("uart", { PIN_OUTPUT_PULLDOWN | 4 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 4 }).addMode("ttyS5-rx", { PIN_OUTPUT_PULLDOWN | 4 });
    addBall("LCD_DATA15", MAIN_DOMAIN, 0x8dc);
    addBall("LCD_VSYNC", MAIN_DOMAIN, 0x8e0, 5, 6);
    addBall("LCD_HSYNC", MAIN_DOMAIN, 0x8e4, 5, 6);
    addBall("LCD_PCLK", MAIN_DOMAIN, 0x8e8, 5, 6);
    addBall("LCD_AC_BIAS_EN", MAIN_DOMAIN, 0x8ec, 5, 6);
    addBall("MMC0_DAT3", MAIN_DOMAIN, 0x8f0);
    addBall("MMC0_DAT2", MAIN_DOMAIN, 0x8f4);
    addBall("MMC0_DAT1", MAIN_DOMAIN, 0x8f8);
    addBall("MMC0_DAT0", MAIN_DOMAIN, 0x8fc);
    addBall("MMC0_CLK", MAIN_DOMAIN, 0x900);
    addBall("MMC0_CMD", MAIN_DOMAIN, 0x904);
    addBall("MII1_COL", MAIN_DOMAIN, 0x908);
    addBall("MII1_CRS", MAIN_DOMAIN, 0x90c);
    addBall("MII1_RX_ER", MAIN_DOMAIN, 0x910);
    addBall("MII1_TX_EN", MAIN_DOMAIN, 0x914);
    addBall("MII1_RX_DV", MAIN_DOMAIN, 0x918);
    addBall("MII1_TXD3", MAIN_DOMAIN, 0x91c);
    addBall("MII1_TXD2", MAIN_DOMAIN, 0x920);
    addBall("MII1_TXD1", MAIN_DOMAIN, 0x924);
    addBall("MII1_TXD0", MAIN_DOMAIN, 0x928);
    addBall("MII1_TX_CLK", MAIN_DOMAIN, 0x92c);
    addBall("MII1_RX_CLK", MAIN_DOMAIN, 0x930);
    addBall("MII1_RXD3", MAIN_DOMAIN, 0x934);
    addBall("MII1_RXD2", MAIN_DOMAIN, 0x938);
    addBall("MII1_RXD1", MAIN_DOMAIN, 0x93c);
    addBall("MII1_RXD0", MAIN_DOMAIN, 0x940);
    addBall("RMII1_REF_CLK", MAIN_DOMAIN, 0x944);
    addBall("MDIO", MAIN_DOMAIN, 0x948);
    addBall("MDC", MAIN_DOMAIN, 0x94c);
    addBall("SPI0_SCLK", MAIN_DOMAIN, 0x950).addMode("uart", { PIN_INPUT_PULLUP | 1 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 1 }).addMode("ttyS2-rx", { PIN_INPUT_PULLUP | 1 }).addMode("spi", { PIN_INPUT_PULLDOWN | 0 }).addMode("spi_clk", { PIN_INPUT_PULLDOWN | 0 });
    addBall("SPI0_D0", MAIN_DOMAIN, 0x954).addMode("uart", { PIN_OUTPUT_PULLDOWN | 1 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 1 }).addMode("ttyS2-tx", { PIN_OUTPUT_PULLDOWN | 1 }).addMode("spi", { PIN_INPUT_PULLDOWN | 0 }).addMode("spi_miso", { PIN_INPUT_PULLDOWN | 0 });
    addBall("SPI0_D1", MAIN_DOMAIN, 0x958).addMode("i2c", { PIN_INPUT_PULLUP | 2 }).addMode("i2c_sda", { PIN_INPUT_PULLUP | 2 }).addMode("spi", { PIN_INPUT_PULLUP | 0 }).addMode("spi_mosi", { PIN_INPUT_PULLUP | 0 });
    addBall("SPI0_CS0", MAIN_DOMAIN, 0x95c).addMode("i2c", { PIN_INPUT_PULLUP | 2 }).addMode("i2c_scl", { PIN_INPUT_PULLUP | 2 }).addMode("spi", { PIN_INPUT_PULLUP | 0 }).addMode("spi_cs0", { PIN_INPUT_PULLUP | 0 });
    addBall("SPI0_CS1", MAIN_DOMAIN, 0x960);
    addBall("ECAP0_IN_PWM0_OUT", MAIN_DOMAIN, 0x964).addMode("uart", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("ttyS3-tx", { PIN_OUTPUT_PULLDOWN | 0 });
    addBall("UART0_CTSN", MAIN_DOMAIN, 0x968).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_sda", { PIN_INPUT_PULLUP | 3 }).addMode("uart", { PIN_INPUT_PULLUP | 0 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 0 }).addMode("ttyS4-rx", { PIN_INPUT_PULLUP | 0 });
    addBall("UART0_RTSN", MAIN_DOMAIN, 0x96c).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_sda", { PIN_INPUT_PULLUP | 3 }).addMode("uart", { PIN_OUTPUT | 0 }).addMode("uart_tx", { PIN_OUTPUT | 0 }).addMode("ttyS4-tx", { PIN_OUTPUT | 0 });
    addBall("UART0_RXD", MAIN_DOMAIN, 0x970, 5, 6).addMode("uart", { PIN_INPUT_PULLUP | 0 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 0 }).addMode("ttyS0-rx", { PIN_INPUT_PULLUP | 0 }).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_sda", { PIN_INPUT_PULLUP | 3 });
    addBall("UART0_TXD", MAIN_DOMAIN, 0x974, 5, 6).addMode("uart", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("ttyS0-tx", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_scl", { PIN_INPUT_PULLUP | 3 });
    addBall("UART1_CTSN", MAIN_DOMAIN, 0x978).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_sda", { PIN_INPUT_PULLUP | 3 });
    addBall("UART1_RTSN", MAIN_DOMAIN, 0x97c).addMode("i2c", { PIN_INPUT_PULLUP | 3 }).addMode("i2c_scl", { PIN_INPUT_PULLUP | 3 });
    addBall("UART1_RXD", MAIN_DOMAIN, 0x980, -1, 6).addMode("uart", { PIN_INPUT_PULLUP | 0 }).addMode("uart_rx", { PIN_INPUT_PULLUP | 0 }).addMode("ttyS1-rx", { PIN_INPUT_PULLUP | 0 });
    addBall("UART1_TXD", MAIN_DOMAIN, 0x984, -1, 6).addMode("uart", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("uart_tx", { PIN_OUTPUT_PULLDOWN | 0 }).addMode("ttyS1-tx", { PIN_OUTPUT_PULLDOWN | 0 });
    addBall("I2C0_SDA", MAIN_DOMAIN, 0x988);
    addBall("I2C0_SCL", MAIN_DOMAIN, 0x98c);
    addBall("MCASP0_ACLKX", MAIN_DOMAIN, 0x990, 5, 6).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 1 });
    addBall("MCASP0_FSX", MAIN_DOMAIN, 0x994, 5, 6).addMode("pwm", { PIN_OUTPUT_PULLDOWN | 1 });
    addBall("MCASP0_AXR0", MAIN_DOMAIN, 0x998, 5, 6);
    addBall("MCASP0_AHCLKR", MAIN_DOMAIN, 0x99c, 5, 6);
    addBall("MCASP0_ACLKR", MAIN_DOMAIN, 0x9a0, 5, 6);
    addBall("MCASP0_FSR", MAIN_DOMAIN, 0x9a4, 5, 6);
    addBall("MCASP0_AXR1", MAIN_DOMAIN, 0x9a8, 5, 6);
    addBall("MCASP0_AHCLKX", MAIN_DOMAIN, 0x9ac, 5, 6);
    addBall("XDMA_EVENT_INTR0", MAIN_DOMAIN, 0x9b0);
    addBall("XDMA_EVENT_INTR1", MAIN_DOMAIN, 0x9b4, -1, 5);
    addBall("WARMRSTN", MAIN_DOMAIN, 0x9b8);
    addBall("NNMI", MAIN_DOMAIN, 0x9c0);
    addBall("TMS", MAIN_DOMAIN, 0x9d0);
    addBall("TDI", MAIN_DOMAIN, 0x9d4);
    addBall("TDO", MAIN_DOMAIN, 0x9d8);
    addBall("TCK", MAIN_DOMAIN, 0x9dc);
    addBall("TRSTN", MAIN_DOMAIN, 0x9e0);
    addBall("EMU0", MAIN_DOMAIN, 0x9e4);
    addBall("EMU1", MAIN_DOMAIN, 0x9e8);
    addBall("RTC_PWRONRSTN", MAIN_DOMAIN, 0x9f8);
    addBall("PMIC_POWER_EN", MAIN_DOMAIN, 0x9fc);
    addBall("EXT_WAKEUP", MAIN_DOMAIN, 0xa00);
    addBall("USB0_DRVVBUS", MAIN_DOMAIN, 0xa1c);
    addBall("USB1_DRVVBUS", MAIN_DOMAIN, 0xa34);
}