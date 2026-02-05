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
    Pin::addPin("P8_07").addBall("GPMC_ADVN_ALE").modesFromBall();
    Pin::addPin("P8_08").addBall("GPMC_OEN_REN").modesFromBall();
    Pin::addPin("P8_09").addBall("GPMC_BEN0_CLE").modesFromBall();
    Pin::addPin("P8_10").addBall("GPMC_WEN").modesFromBall();
    Pin::addPin("P8_11").addBall("GPMC_AD13").modesFromBall();
    Pin::addPin("P8_12").addBall("GPMC_AD12").modesFromBall();
    Pin::addPin("P8_13").addBall("GPMC_AD9").modesFromBall();
    Pin::addPin("P8_14").addBall("GPMC_AD10").modesFromBall();
    Pin::addPin("P8_15").addBall("GPMC_AD15").modesFromBall();
    Pin::addPin("P8_16").addBall("GPMC_AD14").modesFromBall();
    Pin::addPin("P8_17").addBall("GPMC_AD11").modesFromBall();
    Pin::addPin("P8_18").addBall("GPMC_CLK").modesFromBall();
    Pin::addPin("P8_19").addBall("GPMC_AD8").modesFromBall();
    // Pin::addPin("P8-20").addBall("").modesFromBall();
    // Pin::addPin("P8-21").addBall("").modesFromBall();
    // Pin::addPin("P8-22").addBall("").modesFromBall();
    // Pin::addPin("P8-23").addBall("").modesFromBall();
    // Pin::addPin("P8-24").addBall("").modesFromBall();
    // Pin::addPin("P8-25").addBall("").modesFromBall();
    // Pin::addPin("P8-26").addBall("").modesFromBall();
    Pin::addPin("P8_27").addBall("LCD_VSYNC").modesFromBall();
    Pin::addPin("P8_28").addBall("LCD_PLCK").modesFromBall();
    Pin::addPin("P8_29").addBall("LCD_HSYNC").modesFromBall();
    Pin::addPin("P8_30").addBall("LCD_AC_BIAS_EN").modesFromBall();
    Pin::addPin("P8_31").addBall("LCD_DATA14").modesFromBall();
    Pin::addPin("P8_32").addBall("LCD_DATA15").modesFromBall();
    Pin::addPin("P8_33").addBall("LCD_DATA13").modesFromBall();
    Pin::addPin("P8_34").addBall("LCD_DATA11").modesFromBall();
    Pin::addPin("P8_35").addBall("LCD_DATA12").modesFromBall();
    Pin::addPin("P8_36").addBall("LCD_DATA10").modesFromBall();
    Pin::addPin("P8_37").addBall("LCD_DATA8").modesFromBall();
    Pin::addPin("P8_38").addBall("LCD_DATA9").modesFromBall();
    Pin::addPin("P8_39").addBall("LCD_DATA6").modesFromBall();
    Pin::addPin("P8_40").addBall("LCD_DATA7").modesFromBall();
    Pin::addPin("P8_41").addBall("LCD_DATA4").modesFromBall();
    Pin::addPin("P8_42").addBall("LCD_DATA5").modesFromBall();
    Pin::addPin("P8_43").addBall("LCD_DATA2").modesFromBall();
    Pin::addPin("P8_44").addBall("LCD_DATA3").modesFromBall();
    Pin::addPin("P8_45").addBall("LCD_DATA0").modesFromBall();
    Pin::addPin("P8_46").addBall("LCD_DATA1").modesFromBall();
    Pin::addPin("P9_11").addBall("GPMC_WAIT0").modesFromBall();
    Pin::addPin("P9_12").addBall("GPMC_BEN1").modesFromBall();
    Pin::addPin("P9_13").addBall("GPMC_WPN").modesFromBall();
    Pin::addPin("P9_14").addBall("GPMC_A2").modesFromBall();
    Pin::addPin("P9_15").addBall("GPMC_A0").modesFromBall();
    Pin::addPin("P9_16").addBall("GPMC_A3").modesFromBall();
    Pin::addPin("P9_17").addBall("SPI0_CS0").modesFromBall();
    Pin::addPin("P9_18").addBall("SPI0_D1").modesFromBall();
    Pin::addPin("P9_19").addBall("UART1_RTSN").modesFromBall();
    Pin::addPin("P9_20").addBall("UART1_CTSN").modesFromBall();
    Pin::addPin("P9_21").addBall("SPI0_D0").modesFromBall();
    Pin::addPin("P9_22").addBall("SPI0_SCLK").modesFromBall();
    Pin::addPin("P9_23").addBall("GPMC_A1").modesFromBall();
    Pin::addPin("P9_24").addBall("UART1_TXD").modesFromBall();
    Pin::addPin("P9_25").addBall("MCASP0_AHCLKX").modesFromBall();
    Pin::addPin("P9_26").addBall("UART1_RXD").modesFromBall();
    Pin::addPin("P9_27").addBall("MCASP0_FSR").modesFromBall();
    Pin::addPin("P9_28").addBall("MCASP0_AHCLKR").modesFromBall();
    Pin::addPin("P9_29").addBall("MCASP0_FSX").modesFromBall();
    Pin::addPin("P9_30").addBall("MCASP0_AXR0").modesFromBall();
    Pin::addPin("P9_31").addBall("MCASP0_ACLKX").modesFromBall();
    Pin::addPin("P9_41").addBall("XDMA_EVENT_INTR1").modesFromBall();
    Pin::addPin("P9_91").addBall("MCASP0_AXR1").modesFromBall();
    Pin::addPin("P9_42").addBall("ECAP0_IN_PWM0_OUT").modesFromBall();
    Pin::addPin("P9_92").addBall("MCASP0_ACLKR").modesFromBall();
}
