#pragma once

#include <functional>
#include <string>
#include <vector>

#include "DisplayDriver.h"

class OLEDPage {
public:
    static bool InitializeDisplay(int type);

    enum class OLEDType {
        NONE,
        SMALL,
        SINGLE_COLOR,
        TWO_COLOR,
        TEXT_ONLY
    };

    static void SetOLEDType(OLEDType tp);
    static OLEDType GetOLEDType() { return oledType; }
    static void SetHas4DirectionControls(bool b = true) { has4DirectionControls = b; }
    static bool Has4DirectionControls() { return has4DirectionControls; }
    static void SetOLEDOrientationFlipped(bool b) { oledFlipped = b; }
    static bool GetOLEDOrientationFlipped() { return oledFlipped; }
    static OLEDPage* GetCurrentPage() { return currentPage; }
    static void SetCurrentPage(OLEDPage* p);
    static void SetForcedOff(bool b) { oledForcedOff = b; };
    static bool IsForcedOff() { return oledForcedOff; }
    static int GetLEDDisplayWidth();
    static int GetLEDDisplayHeight();
    static int GetI2CBus();

    OLEDPage();
    virtual ~OLEDPage() {}

    OLEDPage(const OLEDPage&) = delete;
    void operator=(OLEDPage const&) = delete;

    virtual void displaying() {}
    virtual void hiding() {}
    virtual void display() {}

    virtual bool doIteration(bool& displayOn) { return false; }
    virtual bool doAction(const std::string& action) = 0;

    OLEDPage& autoDelete() {
        autoDeleteOnHide = true;
        return *this;
    }

    static void printString(int x, int y, const std::string& str, bool white = true);
    static void printStringCentered(int y, const std::string& str, bool white = true);
    static void fillTriangle(short x0, short y0, short x1, short y1, short x2, short y2, bool white = true);
    static void drawLine(short x0, short y0, short x1, short y1, bool white = true);
    static void drawBitmap(short x, short y, const unsigned char bitmap[], short w, short h, bool white = true);
    static void drawRect(short x0, short y0, short x1, short y1, bool white = true);
    static void fillRect(short x0, short y0, short x1, short y1, bool white = true);
    static void flushDisplay();
    static void clearDisplay();
    static void displayOff();

    static int getMinimumRefresh();
    static void displayBootingNotice();

protected:
    static OLEDType oledType;
    static bool oledFlipped;
    static OLEDPage* currentPage;
    static bool oledForcedOff;
    static bool has4DirectionControls;

    bool autoDeleteOnHide;
};

class TitledOLEDPage : public OLEDPage {
public:
    TitledOLEDPage(const std::string& title);
    virtual ~TitledOLEDPage() {}

protected:
    virtual int displayTitle();
    std::string title;
    int numRows;
};

class PromptOLEDPage : public TitledOLEDPage {
public:
    PromptOLEDPage(const std::string& title, const std::string& msg1, const std::string& msg2,
                   const std::vector<std::string>& items);
    PromptOLEDPage(const std::string& title, const std::string& msg1, const std::string& msg2,
                   const std::vector<std::string>& items,
                   const std::function<void(const std::string&)>& itemSelectedCallback);
    virtual ~PromptOLEDPage() {}

    virtual void displaying() override;
    virtual bool doAction(const std::string& action) override;
    virtual void ItemSelected(const std::string& item);

    virtual void display() override;

protected:
    std::string msg1;
    std::string msg2;
    std::vector<std::string> items;
    int curSelected;
    std::function<void(const std::string&)> itemSelectedCallback;
};

class ListOLEDPage : public TitledOLEDPage {
public:
    ListOLEDPage(const std::string& title, const std::vector<std::string>& items, OLEDPage* parent = nullptr);
    virtual ~ListOLEDPage() {}

    virtual void displaying() override;
    virtual bool doAction(const std::string& action) override;

    virtual void display() override;

protected:
    virtual void displayScrollArrows(int startY);

    std::vector<std::string> items;
    int curTop;
    OLEDPage* parent;
};

class MenuOLEDPage : public ListOLEDPage {
public:
    MenuOLEDPage(const std::string& title, const std::vector<std::string>& items, OLEDPage* parent = nullptr);
    MenuOLEDPage(const std::string& title, const std::vector<std::string>& items,
                 const std::function<void(const std::string&)>& itemSelectedCallback,
                 OLEDPage* parent = nullptr);
    virtual ~MenuOLEDPage() {}

    virtual void displaying() override;
    virtual bool doAction(const std::string& action) override;

    virtual void itemSelected(const std::string& item);

    virtual void display() override;

protected:
    int curSelected;
    std::function<void(const std::string&)> itemSelectedCallback;
};
