

#include "Ball.h"
#include "Pin.h"

extern void InitAM6232Balls();

void InitPocketBeagle2() {
    InitAM6232Balls();

    Pin::addPin("P1_02").addBall("E18").addBall("E18").addBall("AA19");
    Pin::addPin("P1_03").addBall("F18");
    Pin::addPin("P1_04").addBall("Y18").addBall("D20");
    Pin::addPin("P1_06").addBall("E19").addBall("AD18");
    Pin::addPin("P1_08").addBall("A20");
    Pin::addPin("P1_10").addBall("A18").addBall("B19");
    Pin::addPin("P1_12").addBall("A19").addBall("AE18");
    Pin::addPin("P1_13").addBall("N20");
    Pin::addPin("P1_19").addBall("AD22");
    Pin::addPin("P1_20").addBall("Y24");
    Pin::addPin("P1_21").addBall("AE22");
    Pin::addPin("P1_23").addBall("AC21");
    Pin::addPin("P1_25").addBall("AB20");
    Pin::addPin("P1_26").addBall("D6").addBall("K24");
    Pin::addPin("P1_27").addBall("AE23");
    Pin::addPin("P1_28").addBall("B3").addBall("K22");
    Pin::addPin("P1_29").addBall("Y20");
    Pin::addPin("P1_30").addBall("E14");
    Pin::addPin("P1_31").addBall("Y22");
    Pin::addPin("P1_32").addBall("D14");
    Pin::addPin("P1_33").addBall("A17").addBall("AA23");
    Pin::addPin("P1_34").addBall("AD23");
    Pin::addPin("P1_35").addBall("AE21");
    Pin::addPin("P1_36").addBall("V20").addBall("B17");

    Pin::addPin("P2_01").addBall("AD24").addBall("B20");
    Pin::addPin("P2_02").addBall("U22");
    Pin::addPin("P2_03").addBall("AB22").addBall("B18");
    Pin::addPin("P2_04").addBall("V24");
    Pin::addPin("P2_05").addBall("C15").addBall("B5").modesFromBall("C15").addMode("uart", "C15", "uart5_rx");
    Pin::addPin("P2_06").addBall("W25").modesFromBall();
    Pin::addPin("P2_07").addBall("E15").addBall("A5").modesFromBall("E15").addMode("uart", "E15", "uart5_tx");
    Pin::addPin("P2_08").addBall("W24").modesFromBall();
    Pin::addPin("P2_09").addBall("A15").addBall("D4").modesFromBall("A15");
    Pin::addPin("P2_10").addBall("AD21").modesFromBall();
    Pin::addPin("P2_11").addBall("B15").addBall("E5").modesFromBall("B15");
    Pin::addPin("P2_17").addBall("AC24").modesFromBall();
    Pin::addPin("P2_18").addBall("V21").modesFromBall();
    Pin::addPin("P2_19").addBall("AC20");
    Pin::addPin("P2_20").addBall("Y25");
    Pin::addPin("P2_22").addBall("AC25").modesFromBall();
    Pin::addPin("P2_24").addBall("Y23");
    Pin::addPin("P2_25").addBall("B14");
    Pin::addPin("P2_27").addBall("B13");
    Pin::addPin("P2_28").addBall("AB24");
    Pin::addPin("P2_29").addBall("M22").addBall("A14").modesFromBall("M22");
    Pin::addPin("P2_30").addBall("AA24").modesFromBall();
    Pin::addPin("P2_31").addBall("AA18").addBall("A13").modesFromBall("AA18");
    Pin::addPin("P2_32").addBall("AB25");
    Pin::addPin("P2_33").addBall("AA25").modesFromBall().addMode("uart", "AA25", "uart5_tx");
    Pin::addPin("P2_34").addBall("AA21").modesFromBall();
    Pin::addPin("P2_35").addBall("W21").modesFromBall();
    Pin::addPin("P2_36").addBall("C13");

    /*
        Pin::addPin("I2C0_SCL").addBall("B16");
        Pin::addPin("I2C0_SDA").addBall("A16");
        Pin::addPin("WKUP_I2C0_SCL").addBall("B9");
        Pin::addPin("WKUP_I2C0_SDA").addBall("A9");
        Pin::addPin("I2C1_SCL").addBall("B17");
        Pin::addPin("I2C1_SDA").addBall("A17");
    */
}
