#include <cstring>

#include "Ball.h"
#include "Pin.h"

extern void InitAM335xBalls();

void InitPocketBeagle1() {
    InitAM335xBalls();

    Pin::addPin("P1_02").addBall("LCD_HSYNC").modesFromBall();
    Pin::addPin("P1_04").addBall("LCD_AC_BIAS_EN").modesFromBall();
    Pin::addPin("P1_06").addBall("SPI0_CS0").modesFromBall();
    Pin::addPin("P1_08").addBall("SPI0_SCLK").modesFromBall();
    Pin::addPin("P1_10").addBall("SPI0_D0").modesFromBall();
    Pin::addPin("P1_12").addBall("SPI0_D1").modesFromBall();
    Pin::addPin("P1_20").addBall("XDMA_EVENT_INTR1").modesFromBall();
    Pin::addPin("P1_26").addBall("UART1_CTSN").modesFromBall();
    Pin::addPin("P1_28").addBall("UART1_RTSN").modesFromBall();
    Pin::addPin("P1_29").addBall("MCASP0_AHCLKX").modesFromBall();
    Pin::addPin("P1_30").addBall("UART0_TXD").modesFromBall();
    Pin::addPin("P1_31").addBall("MCASP0_ACLKR").modesFromBall();
    Pin::addPin("P1_32").addBall("UART0_RXD").modesFromBall();
    Pin::addPin("P1_33").addBall("MCASP0_FSX").modesFromBall();
    Pin::addPin("P1_34").addBall("GPMC_AD10").modesFromBall();
    Pin::addPin("P1_35").addBall("LCD_PCLK").modesFromBall();
    Pin::addPin("P1_36").addBall("MCASP0_ACLKX").modesFromBall();

    Pin::addPin("P2_01").addBall("GPMC_A2").modesFromBall();
    Pin::addPin("P2_02").addBall("GPMC_A11").modesFromBall();
    Pin::addPin("P2_03").addBall("GPMC_AD9").modesFromBall();
    Pin::addPin("P2_04").addBall("GPMC_A10").modesFromBall();
    Pin::addPin("P2_05").addBall("GPMC_WAIT0").modesFromBall();
    Pin::addPin("P2_06").addBall("GPMC_A9").modesFromBall();
    Pin::addPin("P2_07").addBall("GPMC_WPN").modesFromBall();
    Pin::addPin("P2_08").addBall("GPMC_BEN1").modesFromBall();
    Pin::addPin("P2_09").addBall("UART1_TXD").modesFromBall();
    Pin::addPin("P2_10").addBall("GPMC_A4").modesFromBall();
    Pin::addPin("P2_11").addBall("UART1_RXD").modesFromBall();
    Pin::addPin("P2_17").addBall("GPMC_CLK").modesFromBall();
    Pin::addPin("P2_18").addBall("GPMC_AD15").modesFromBall();
    Pin::addPin("P2_19").addBall("GPMC_AD11").modesFromBall();
    Pin::addPin("P2_20").addBall("GPMC_CSN3").modesFromBall();
    Pin::addPin("P2_22").addBall("GPMC_AD14").modesFromBall();
    Pin::addPin("P2_24").addBall("GPMC_AD12").modesFromBall();
    Pin::addPin("P2_25").addBall("UART0_RTSN").modesFromBall();
    Pin::addPin("P2_27").addBall("UART0_CTSN").modesFromBall();
    Pin::addPin("P2_28").addBall("MCASP0_AXR1").modesFromBall();
    Pin::addPin("P2_29").addBall("ECAP0_IN_PWM0_OUT").modesFromBall();
    Pin::addPin("P2_30").addBall("MCASP0_AHCLKR").modesFromBall();
    Pin::addPin("P2_31").addBall("XDMA_EVENT_INTR0").modesFromBall();
    Pin::addPin("P2_32").addBall("MCASP0_AXR0").modesFromBall();
    Pin::addPin("P2_33").addBall("GPMC_AD13").modesFromBall();
    Pin::addPin("P2_34").addBall("MCASP0_FSR").modesFromBall();
    Pin::addPin("P2_35").addBall("LCD_VSYNC").modesFromBall();

    /*
        Pin::addPin("I2C0_SCL").addBall("B16");
        Pin::addPin("I2C0_SDA").addBall("A16");
        Pin::addPin("WKUP_I2C0_SCL").addBall("B9");
        Pin::addPin("WKUP_I2C0_SDA").addBall("A9");
        Pin::addPin("I2C1_SCL").addBall("B17");
        Pin::addPin("I2C1_SDA").addBall("A17");
    */
}
