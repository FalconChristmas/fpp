#include <cstring>

#include "Ball.h"
#include "Pin.h"

extern void InitAM335xBalls();

void InitBeagleBoneBlack() {
    InitAM335xBalls();

    // Pin::addPin("P8-03").addBall("").modesFromBall();
    // Pin::addPin("P8-04").addBall("").modesFromBall();
    // Pin::addPin("P8-05").addBall("").modesFromBall();
    // Pin::addPin("P8-06").addBall("").modesFromBall();
    Pin::addPin("P8-07").addBall("GPMC_ADVN_ALE").modesFromBall();
    Pin::addPin("P8-08").addBall("GPMC_OEN_REN").modesFromBall();
    Pin::addPin("P8-09").addBall("GPMC_BEN0_CLE").modesFromBall();
    Pin::addPin("P8-10").addBall("GPMC_WEN").modesFromBall();
    Pin::addPin("P8-11").addBall("GPMC_AD13").modesFromBall();
    Pin::addPin("P8-12").addBall("GPMC_AD12").modesFromBall();
    Pin::addPin("P8-13").addBall("GPMC_AD9").modesFromBall();
    Pin::addPin("P8-14").addBall("GPMC_AD10").modesFromBall();
    Pin::addPin("P8-15").addBall("GPMC_AD15").modesFromBall();
    Pin::addPin("P8-16").addBall("GPMC_AD14").modesFromBall();
    Pin::addPin("P8-17").addBall("GPMC_AD11").modesFromBall();
    Pin::addPin("P8-18").addBall("GPMC_CLK").modesFromBall();
    Pin::addPin("P8-19").addBall("GPMC_AD8").modesFromBall();
    // Pin::addPin("P8-20").addBall("").modesFromBall();
    // Pin::addPin("P8-21").addBall("").modesFromBall();
    // Pin::addPin("P8-22").addBall("").modesFromBall();
    // Pin::addPin("P8-23").addBall("").modesFromBall();
    // Pin::addPin("P8-24").addBall("").modesFromBall();
    // Pin::addPin("P8-25").addBall("").modesFromBall();
    // Pin::addPin("P8-26").addBall("").modesFromBall();
    Pin::addPin("P8-27").addBall("LCD_VSYNC").modesFromBall();
    Pin::addPin("P8-28").addBall("LCD_PLCK").modesFromBall();
    Pin::addPin("P8-29").addBall("LCD_HSYNC").modesFromBall();
    Pin::addPin("P8-30").addBall("LCD_AC_BIAS_EN").modesFromBall();
    Pin::addPin("P8-31").addBall("LCD_DATA14").modesFromBall();
    Pin::addPin("P8-32").addBall("LCD_DATA15").modesFromBall();
    Pin::addPin("P8-33").addBall("LCD_DATA13").modesFromBall();
    Pin::addPin("P8-34").addBall("LCD_DATA11").modesFromBall();
    Pin::addPin("P8-35").addBall("LCD_DATA12").modesFromBall();
    Pin::addPin("P8-36").addBall("LCD_DATA10").modesFromBall();
    Pin::addPin("P8-37").addBall("LCD_DATA8").modesFromBall();
    Pin::addPin("P8-38").addBall("LCD_DATA9").modesFromBall();
    Pin::addPin("P8-39").addBall("LCD_DATA6").modesFromBall();
    Pin::addPin("P8-40").addBall("LCD_DATA7").modesFromBall();
    Pin::addPin("P8-41").addBall("LCD_DATA4").modesFromBall();
    Pin::addPin("P8-42").addBall("LCD_DATA5").modesFromBall();
    Pin::addPin("P8-43").addBall("LCD_DATA2").modesFromBall();
    Pin::addPin("P8-44").addBall("LCD_DATA3").modesFromBall();
    Pin::addPin("P8-45").addBall("LCD_DATA0").modesFromBall();
    Pin::addPin("P8-46").addBall("LCD_DATA1").modesFromBall();
    Pin::addPin("P9-11").addBall("GPMC_WAIT0").modesFromBall();
    Pin::addPin("P9-12").addBall("GPMC_BEN1").modesFromBall();
    Pin::addPin("P9-13").addBall("GPMC_WPN").modesFromBall();
    Pin::addPin("P9-14").addBall("GPMC_A2").modesFromBall();
    Pin::addPin("P9-15").addBall("GPMC_A0").modesFromBall();
    Pin::addPin("P9-16").addBall("GPMC_A3").modesFromBall();
    Pin::addPin("P9-17").addBall("SPI0_CS0").modesFromBall();
    Pin::addPin("P9-18").addBall("SPI0_D1").modesFromBall();
    Pin::addPin("P9-19").addBall("UART1_RTSN").modesFromBall();
    Pin::addPin("P9-20").addBall("UART1_CTSN").modesFromBall();
    Pin::addPin("P9-21").addBall("SPI0_D0").modesFromBall();
    Pin::addPin("P9-22").addBall("SPI0_SCLK").modesFromBall();
    Pin::addPin("P9-23").addBall("GPMC_A1").modesFromBall();
    Pin::addPin("P9-24").addBall("UART1_TXD").modesFromBall();
    Pin::addPin("P9-25").addBall("MCASP0_AHCLKX").modesFromBall();
    Pin::addPin("P9-26").addBall("UART1_RXD").modesFromBall();
    Pin::addPin("P9-27").addBall("MCASP0_FSR").modesFromBall();
    Pin::addPin("P9-28").addBall("MCASP0_AHCLKR").modesFromBall();
    Pin::addPin("P9-29").addBall("MCASP0_FSX").modesFromBall();
    Pin::addPin("P9-30").addBall("MCASP0_AXR0").modesFromBall();
    Pin::addPin("P9-31").addBall("MCASP0_ACLKX").modesFromBall();
    Pin::addPin("P9-41").addBall("XDMA_EVENT_INTR1").modesFromBall();
    Pin::addPin("P9-91").addBall("MCASP0_AXR1").modesFromBall();
    Pin::addPin("P9-42").addBall("ECAP0_IN_PWM0_OUT").modesFromBall();
    Pin::addPin("P9-92").addBall("MCASP0_ACLKR").modesFromBall();
}
