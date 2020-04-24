#include "fpp-pch.h"
#include "OLEDPages.h"

static std::unique_ptr<DisplayDriver> displayDriver { nullptr };

OLEDPage::OLEDType OLEDPage::oledType = OLEDPage::OLEDType::SINGLE_COLOR;
bool OLEDPage::oledFlipped = false;
OLEDPage *OLEDPage::currentPage = nullptr;
bool OLEDPage::oledForcedOff = false;
bool OLEDPage::has4DirectionControls = false;

static int DISPLAY_I2CBUS = 0;


bool OLEDPage::InitializeDisplay(int ledType) {
    if (ledType == 0) {
        SetOLEDType(OLEDPage::OLEDType::NONE);
        return false;
    }
    if (ledType <= 10) {
        displayDriver = std::make_unique<SSD1306DisplayDriver>(ledType);
        if (!displayDriver->initialize(DISPLAY_I2CBUS)) {
            displayDriver.reset(nullptr);
            SetOLEDType(OLEDPage::OLEDType::NONE);
            return false;
        }
        
        if (ledType == 3 || ledType == 4) {
            SetOLEDType(OLEDPage::OLEDType::SMALL);
        } else if (ledType == 7 || ledType == 8) {
            SetOLEDType(OLEDPage::OLEDType::TWO_COLOR);
        } else {
            SetOLEDType(OLEDPage::OLEDType::SINGLE_COLOR);
        }
        if (ledType == 2 || ledType == 4 || ledType == 6 || ledType == 8 || ledType == 10) {
            SetOLEDOrientationFlipped(true);
        }
        
        return true;
    } else if (ledType <= 20) {
        displayDriver = std::make_unique<I2C1602_2004_DisplayDriver>(ledType);
        if (!displayDriver->initialize(DISPLAY_I2CBUS)) {
            displayDriver.reset(nullptr);
            SetOLEDType(OLEDPage::OLEDType::NONE);
            return false;
        }
        SetOLEDType(OLEDPage::OLEDType::TEXT_ONLY);
    }
    return false;
}

int OLEDPage::GetI2CBus() {
    return DISPLAY_I2CBUS;
}

void OLEDPage::SetOLEDType(OLEDType tp) {
    oledType = tp;
}

void OLEDPage::flushDisplay() {
    if (displayDriver) {
        displayDriver->flushDisplay();
    }
}
void OLEDPage::clearDisplay() {
    if (displayDriver) {
        displayDriver->clearDisplay();
    }
}
void OLEDPage::fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white) {
    if (displayDriver) {
        displayDriver->fillTriangle(x0, y0, x1, y1, x2, y2, white);
    }
}
void OLEDPage::drawLine(short x0, short y0, short x1, short y1, bool white) {
    if (displayDriver) {
        displayDriver->drawLine(x0, y0, x1, y1, white);
    }
}
void OLEDPage::drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white) {
    if (displayDriver) {
        displayDriver->drawBitmap(x, y, bitmap, w, h, white);
    }
}
void OLEDPage::drawRect(short x0, short y0, short x1, short y1, bool white) {
    if (displayDriver) {
        displayDriver->drawRect(x0, y0, x1, y1, white);
    }
}
void OLEDPage::fillRect(short x0, short y0, short x1, short y1, bool white) {
    if (displayDriver) {
        displayDriver->fillRect(x0, y0, x1, y1, white);
    }
}

void OLEDPage::printString(int x, int y, const std::string &str, bool white) {
    if (oledForcedOff) return;
    if (displayDriver) {
        displayDriver->printString(x, y, str, white);
    }
}
void OLEDPage::printStringCentered(int y, const std::string &str, bool white) {
    if (oledForcedOff) return;
    if (displayDriver) {
        displayDriver->printStringCentered(y, str, white);
    }
}
int OLEDPage::GetLEDDisplayWidth() {
    if (displayDriver) {
        return displayDriver->getWidth();
    }
    return 128;
}
int OLEDPage::GetLEDDisplayHeight() {
    if (displayDriver) {
        return displayDriver->getHeight();
    }
    return 32;
}
void OLEDPage::displayOff() {
    if (displayDriver) {
        return displayDriver->displayOff();
    }
}
int OLEDPage::getMinimumRefresh() {
    if (displayDriver) {
        return displayDriver->getMinimumRefresh();
    }
    return 1000;
}

OLEDPage::OLEDPage() : autoDeleteOnHide(false) {
}

void OLEDPage::SetCurrentPage(OLEDPage *p) {
    if (currentPage) {
        currentPage->hiding();
        if (currentPage->autoDeleteOnHide) {
            delete currentPage;
        }
    }
    currentPage = p;
    if (currentPage) {
        currentPage->displaying();
    }
}


TitledOLEDPage::TitledOLEDPage(const std::string &t) : title(t) {
    numRows = GetLEDDisplayHeight() == 128 ? 11 : 5;
    if (oledType == OLEDPage::OLEDType::TWO_COLOR && oledFlipped) {
        numRows = 4;
    } else if (oledType == OLEDPage::OLEDType::SMALL) {
        numRows = 3;
    } else if (oledType == OLEDPage::OLEDType::TEXT_ONLY) {
        numRows = GetLEDDisplayHeight() / 8 - 1;
    }
}
int TitledOLEDPage::displayTitle() {
    if (oledForcedOff) return 0;
    
    int startY = 0;
    int maxY = 63;
    switch (numRows) {
        case 4:
            maxY = 47;
            break;
        case 3:
            maxY = 31;
            break;
        case 2:
            maxY = 23;
            break;
        case 1:
            maxY = 15;
            break;
        case 11:
            maxY = 127;
            break;
    }
    if (oledType == OLEDPage::OLEDType::TWO_COLOR && !oledFlipped) {
        if (displayDriver && displayDriver->setFont("Roboto_Medium_14")) {
            printStringCentered(startY, title);
            displayDriver->setFont("");
        } else {
            printStringCentered(startY + 4, title);
        }
        startY += 16;
    } else if (oledType == OLEDPage::OLEDType::TEXT_ONLY) {
        int mw = GetLEDDisplayWidth() / 6;
        mw -= title.length();
        std::string s = title;
        if (mw > 4) {
            s = " " + s + " ";
            mw -= 2;
        }
        for (int x = 0; x < mw; x += 2) {
            if (x != (mw - 1)) {
                s = "=" + s + "=";
            } else {
                s += "=";
            }
        }
        printString(0, startY, s);
        startY += 8;
    } else {
        fillRect(0, 0, 128, 8);
        drawRect(0, 0, 128, maxY + 1);
        printStringCentered(startY, title, false);
        if (maxY > 50) {
            startY += 12;
        } else if (maxY > 32) {
            startY += 11;
        } else {
            startY += 8;
        }
    }
    return startY;
}


PromptOLEDPage::PromptOLEDPage(const std::string &t, const std::string &m1, const std::string &m2,
                               const std::vector<std::string> &i)
: TitledOLEDPage(t), msg1(m1), msg2(m2), items(i), curSelected(0), itemSelectedCallback(nullptr) {
}
PromptOLEDPage::PromptOLEDPage(const std::string &t, const std::string &m1, const std::string &m2,
                               const std::vector<std::string> &i,
                               const std::function<void (const std::string &)>& cb)
: TitledOLEDPage(t), msg1(m1), msg2(m2), items(i), curSelected(0), itemSelectedCallback(cb) {
}
void PromptOLEDPage::displaying() {
    curSelected = 0;
    display();
}
void PromptOLEDPage::display() {
    if (oledForcedOff) return;
    clearDisplay();
    int startY = displayTitle();
    int skipY = numRows <= 3 ? 8 : (numRows == 4 ? 9 : 10);
    
    if (numRows > 4) {
        startY += skipY;
    }
    if (numRows != 1) {
        printStringCentered(startY, msg1);
        startY += skipY;
        if (numRows > 2) {
            printStringCentered(startY, msg2);
            startY += skipY;
        }
    }

    if (!items.empty()) {
        int skipX = 122 / items.size();
        for (int x = 0; x < items.size(); x++) {
            if (x == curSelected) {
                fillRect(x * skipX + 4, startY, skipX, 8);
            }
            int l = items[x].length() * 6 / 2;
            printString(x * skipX + 4 + skipX / 2 - l, startY, items[x], x != curSelected);
        }
    }
    flushDisplay();
}
bool PromptOLEDPage::doAction(const std::string &action) {
    if (action == "Down" || action == "Test/Down") {
        curSelected++;
        if (curSelected == items.size()) {
            curSelected = 0;
        }
        display();
    } else if (action == "Up") {
        if (curSelected) {
            curSelected--;
        } else {
            curSelected = items.size() - 1;
        }
        display();
    } else if (action == "Enter") {
        ItemSelected(items[curSelected]);
    }
    return false;
}
void PromptOLEDPage::ItemSelected(const std::string &item) {
    if (itemSelectedCallback) {
        itemSelectedCallback(item);
    }
}


ListOLEDPage::ListOLEDPage(const std::string &t, const std::vector<std::string> &i, OLEDPage *p)
: TitledOLEDPage(t), items(i), curTop(0), parent(p) {
}
void ListOLEDPage::displaying() {
    curTop = 0;
    display();
}
void ListOLEDPage::displayScrollArrows(int startY) {
    int maxY = numRows == 11 ? 127 : (numRows == 5 ? 63 : (numRows == 4 ? 47 : 31));
    if (items.size() > numRows) {
        if (curTop != 0) {
            fillTriangle(116, startY + 8, 125, startY + 8, 120, startY);
        }
        if ((curTop + numRows) < items.size()) {
            fillTriangle(116, maxY - 10, 125, maxY - 10, 120, maxY - 2);
        }
    }
}


void ListOLEDPage::display() {
    if (oledForcedOff) return;
    clearDisplay();
    int startY = displayTitle();
    displayScrollArrows(startY);
    int skipY = numRows <= 3 ? 8 : (numRows == 4 ? 9 : 10);
    int posX = 6;
    if (oledType == OLEDPage::OLEDType::TEXT_ONLY) {
        posX = 0;
    }
    for (int x = curTop; (x < items.size()) && (x < (curTop + numRows)); ++x) {
        printString(posX, startY, items[x]);
        startY += skipY;
    }
    flushDisplay();
}

bool ListOLEDPage::doAction(const std::string &action) {
    if (action == "Down") {
        if ((items.size() - curTop) < numRows) {
            curTop++;
        }
        display();
    } else if (action == "Test/Down") {
        curTop++;
        if ((items.size() - curTop) < numRows) {
            curTop = 0;
        }
        display();
    } else if (action == "Up") {
        if (curTop) {
            curTop--;
        }
        display();
    } else if (action == "Enter" || (action == "Back" && parent)) {
        SetCurrentPage(parent);
    }
    return true;
}


MenuOLEDPage::MenuOLEDPage(const std::string &t, const std::vector<std::string> &i, OLEDPage *p)
: ListOLEDPage(t, i, p), curSelected(0), itemSelectedCallback(nullptr) {
}
MenuOLEDPage::MenuOLEDPage(const std::string &t, const std::vector<std::string> &i,
                           const std::function<void (const std::string &)>& cb,
                           OLEDPage *p)
: ListOLEDPage(t, i, p), curSelected(0), itemSelectedCallback(cb) {
}
void MenuOLEDPage::displaying() {
    curSelected = 0;
    curTop = 0;
    display();
}
void MenuOLEDPage::display() {
    if (oledForcedOff) return;
    clearDisplay();
    int startY = displayTitle();
    displayScrollArrows(startY);
    int skipY = numRows <= 3 ? 8 : (numRows == 4 ? 9 : 10);
    int width = items.size() > numRows ? 110 : 120;
    int posX = 6;
    if (oledType == OLEDPage::OLEDType::TEXT_ONLY) {
        posX = 0;
    }
    for (int x = curTop; (x < items.size()) && (x < (curTop + numRows)); ++x) {
        if (curSelected == x) {
            fillRect(4, startY, width, 8);
        }
        printString(posX, startY, items[x], curSelected != x);
        startY += skipY;
    }
    flushDisplay();
}
bool MenuOLEDPage::doAction(const std::string &action) {
    //printf("In menu action %s\n", action.c_str());
    if (action == "Down" || action == "Test/Down") {
        curSelected++;
        if (curSelected >= items.size()) {
            curSelected = 0;
            curTop = 0;
        }
        while ((curSelected - curTop) >= numRows) {
            curTop++;
        }
        display();
    } else if (action == "Up") {
        if (curSelected) {
            curSelected--;
            if (curSelected < curTop) {
                curTop = curSelected;
            }
        } else {
            curSelected = items.size() - 1;
            curTop = curSelected - numRows + 1;
            if (curTop < 0) {
                curTop = 0;
            }
        }
        display();
    } else if (action == "Enter") {
        itemSelected(items[curSelected]);
    } else if (action == "Back" && parent) {
        SetCurrentPage(parent);
    }
    return true;
}

void MenuOLEDPage::itemSelected(const std::string &item) {
    if (itemSelectedCallback) {
        itemSelectedCallback(item);
    }
}


