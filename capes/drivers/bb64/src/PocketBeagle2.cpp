#include <cstring>

#include "Ball.h"
#include "Pin.h"

extern void InitAM6232Balls();

void InitPocketBeagle2() {
    InitAM6232Balls();

    bool isBeaglePlay = false;
    FILE* file = fopen("/proc/device-tree/model", "r");
    if (file) {
        char buf[256];
        fgets(buf, 256, file);
        fclose(file);
        if (strcmp(buf, "BeagleBoard.org BeaglePlay") == 0) {
            isBeaglePlay = true;
        }
    }

    Pin::addPin("P1_03").addBall("F18").modesFromBall();
    if (!isBeaglePlay) {
        Pin::addPin("P1_02").addBall("E18").addBall("AA19").modesFromBall("E18");
        Pin::addPin("P1_04").addBall("D20").addBall("Y18").modesFromBall("D20");
    } else {
        Pin::addPin("P1_02").addBall("E18").modesFromBall("E18");
        Pin::addPin("P1_04").addBall("D20").modesFromBall("D20");
    }
    Pin::addPin("P1_06").addBall("E19").addBall("AD18").modesFromBall("E19").addMode("uart", "E19", "uart1_rx");
    Pin::addPin("P1_08").addBall("A20").modesFromBall().addMode("uart", "A20", "uart1_tx");                      //
    Pin::addPin("P1_10").addBall("B19").addBall("A18").modesFromBall("B19").addMode("uart", "B19", "uart6_rx");  // b19
    Pin::addPin("P1_12").addBall("A19").addBall("AE18").modesFromBall("A19").addMode("uart", "A19", "uart6_tx"); // a19
    Pin::addPin("P1_13").addBall("N20").modesFromBall();
    Pin::addPin("P1_19").addBall("AD22").modesFromBall();
    Pin::addPin("P1_20").addBall("Y24").modesFromBall().addMode("uart", "Y24", "uart4_tx");
    Pin::addPin("P1_21").addBall("AE22").modesFromBall();
    Pin::addPin("P1_23").addBall("AC21").modesFromBall();
    Pin::addPin("P1_25").addBall("AB20").modesFromBall();
    Pin::addPin("P1_26").addBall("K24").addBall("D6").modesFromBall("K24").addMode("uart", "K24", "uart4_tx");
    Pin::addPin("P1_27").addBall("AE23").modesFromBall();
    Pin::addPin("P1_28").addBall("K22").addBall("B3").modesFromBall("K22").addMode("uart", "K22", "uart4_rx");
    Pin::addPin("P1_29").addBall("Y20").modesFromBall();
    Pin::addPin("P1_30").addBall("E14").modesFromBall().addMode("uart", "E14", "uart0_tx");
    Pin::addPin("P1_31").addBall("Y22").modesFromBall();
    Pin::addPin("P1_32").addBall("D14").modesFromBall().addMode("uart", "D14", "uart0_rx");
    Pin::addPin("P1_33").addBall("AA23").addBall("A17").modesFromBall("AA23").addMode("uart", "A17", "uart1_tx");
    Pin::addPin("P1_34").addBall("AD23").modesFromBall();
    Pin::addPin("P1_35").addBall("AE21").modesFromBall();
    Pin::addPin("P1_36").addBall("V20").addBall("B17").modesFromBall("V20").addMode("uart", "AB17", "uart1_rx");

    Pin::addPin("P2_01").addBall("B20").modesFromBall("B20");
    if (!isBeaglePlay) {
        Pin::getPin("P2_01").addBall("AD24");
    }
    Pin::addPin("P2_02").addBall("U22").modesFromBall().addMode("uart", "U22", "uart2_rx");
    Pin::addPin("P2_03").addBall("B18").modesFromBall("B18");
    if (!isBeaglePlay) {
        Pin::getPin("P2_03").addBall("AB22");
    }
    Pin::addPin("P2_04").addBall("V24").modesFromBall().addMode("uart", "V24", "uart2_tx");
    Pin::addPin("P2_05").addBall("C15").addBall("B5").modesFromBall("C15").addMode("uart", "C15", "uart5_rx");
    Pin::addPin("P2_06").addBall("W25").modesFromBall();
    Pin::addPin("P2_07").addBall("E15").addBall("A5").modesFromBall("E15").addMode("uart", "E15", "uart5_tx");
    Pin::addPin("P2_08").addBall("W24").modesFromBall();
    Pin::addPin("P2_09").addBall("A15").addBall("D4").modesFromBall("A15");
    Pin::addPin("P2_10").addBall("AD21").modesFromBall();
    Pin::addPin("P2_11").addBall("B15").addBall("E5").modesFromBall("B15");
    Pin::addPin("P2_17").addBall("AC24").modesFromBall();
    Pin::addPin("P2_18").addBall("V21").modesFromBall();
    Pin::addPin("P2_19").addBall("AC20").modesFromBall();
    Pin::addPin("P2_20").addBall("Y25").modesFromBall().addMode("uart", "Y25", "uart4_rx");
    Pin::addPin("P2_22").addBall("AC25").modesFromBall();
    Pin::addPin("P2_24").addBall("Y23").modesFromBall().addMode("uart", "Y23", "uart5_rx");
    Pin::addPin("P2_25").addBall("B14").modesFromBall();
    Pin::addPin("P2_27").addBall("B13").modesFromBall();
    Pin::addPin("P2_28").addBall("AB24").modesFromBall();
    Pin::addPin("P2_29").addBall("M22").addBall("A14").modesFromBall("M22");
    Pin::addPin("P2_30").addBall("AA24").modesFromBall();
    Pin::addPin("P2_31").addBall("A13").addBall("AA18").modesFromBall("A13").modesFromBallNoConflicts("AA18");
    Pin::addPin("P2_32").addBall("AB25").modesFromBall();
    Pin::addPin("P2_33").addBall("AA25").modesFromBall().addMode("uart", "AA25", "uart5_tx");
    Pin::addPin("P2_34").addBall("AA21").modesFromBall();
    Pin::addPin("P2_35").addBall("W21").modesFromBall();
    Pin::addPin("P2_36").addBall("C13").modesFromBall();

    /*
        Pin::addPin("I2C0_SCL").addBall("B16");
        Pin::addPin("I2C0_SDA").addBall("A16");
        Pin::addPin("WKUP_I2C0_SCL").addBall("B9");
        Pin::addPin("WKUP_I2C0_SDA").addBall("A9");
        Pin::addPin("I2C1_SCL").addBall("B17");
        Pin::addPin("I2C1_SDA").addBall("A17");
    */
}
