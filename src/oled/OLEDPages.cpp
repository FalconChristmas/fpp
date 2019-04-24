
#include "OLEDPages.h"
#include "SSD1306_OLED.h"

#include "RobotoFont-14.h"

OLEDPage::OLEDType OLEDPage::oledType = OLEDPage::OLEDType::SINGLE_COLOR;
bool OLEDPage::oledFlipped = false;
OLEDPage *OLEDPage::currentPage = nullptr;


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

void OLEDPage::printString(int x, int y, const std::string &str, bool white) {
    setTextColor(white ? WHITE : BLACK);
    setCursor(x, y);
    print_str(str.c_str());
}
void OLEDPage::printStringCentered(int y, const std::string &str, bool white) {
    setTextColor(white ? WHITE : BLACK);
    int len = getTextWidth(str.c_str());
    len /= 2;
    setCursor(64 - len, y);
    print_str(str.c_str());
}

TitledOLEDPage::TitledOLEDPage(const std::string &t) : title(t) {
    numRows = 5;
    if (oledType == OLEDPage::OLEDType::TWO_COLOR && oledFlipped) {
        numRows = 4;
    } else if (oledType == OLEDPage::OLEDType::SMALL) {
        numRows = 3;
    }
}
int TitledOLEDPage::displayTitle() {
    setTextColor(WHITE);
    
    int startY = 0;
    int maxY = numRows == 5 ? 63 : (numRows == 4 ? 47 : 31);
    
    if (oledType == OLEDPage::OLEDType::TWO_COLOR && !oledFlipped) {
        setTextFont(&Roboto_Medium_14);
        printStringCentered(startY, title, WHITE);
        setTextFont(nullptr);
        startY += 16;
    } else {
        fillRect(0, 0, 128, 8, WHITE);
        drawRect(0, 0, 128, maxY + 1, WHITE);
        printStringCentered(startY, title, BLACK);
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
    clearDisplay();
    int startY = displayTitle();
    int skipY = numRows == 3 ? 8 : (numRows == 4 ? 9 : 10);
    setTextSize(1);
    setTextColor(WHITE);
    
    if (numRows > 4) {
        startY += skipY;
    }
    printStringCentered(startY, msg1);
    startY += skipY;
    printStringCentered(startY, msg2);
    startY += skipY;

    if (!items.empty()) {
        int skipX = 122 / items.size();
        for (int x = 0; x < items.size(); x++) {
            if (x == curSelected) {
                fillRect(x * skipX + 4, startY, skipX, 8, WHITE);
            }
            int l = items[x].length() * 6 / 2;
            printString(x * skipX + 4 + skipX / 2 - l, startY, items[x], x != curSelected);
        }
    }
    
    Display();
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
    int maxY = numRows == 5 ? 63 : (numRows == 4 ? 47 : 31);
    if (items.size() > numRows) {
        if (curTop != 0) {
            fillTriangle(116, startY + 8, 125, startY + 8, 120, startY, WHITE);
        }
        if ((curTop + numRows) < items.size()) {
            fillTriangle(116, maxY - 10, 125, maxY - 10, 120, maxY - 2, WHITE);
        }
    }
}


void ListOLEDPage::display() {
    clearDisplay();
    int startY = displayTitle();
    displayScrollArrows(startY);
    int skipY = numRows == 3 ? 8 : (numRows == 4 ? 9 : 10);
    setTextSize(1);
    setTextColor(WHITE);
    for (int x = curTop; (x < items.size()) && (x < (curTop + numRows)); ++x) {
        printString(6, startY, items[x]);
        startY += skipY;
    }
    Display();
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
    } else if (action == "Enter" || action == "Back" && parent) {
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
    clearDisplay();
    int startY = displayTitle();
    displayScrollArrows(startY);
    int skipY = numRows == 3 ? 8 : (numRows == 4 ? 9 : 10);
    int width = items.size() > numRows ? 110 : 120;
    setTextSize(1);
    for (int x = curTop; (x < items.size()) && (x < (curTop + numRows)); ++x) {
        setCursor(6, startY);
        if (curSelected == x) {
            fillRect(4, startY, width, 8, WHITE);
        }
        printString(6, startY, items[x], curSelected != x);
        startY += skipY;
    }
    Display();
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

bool MenuOLEDPage::itemSelected(const std::string &item) {
    if (itemSelectedCallback) {
        itemSelectedCallback(item);
    }
}


