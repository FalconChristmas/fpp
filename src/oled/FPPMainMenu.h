#ifndef  __FPPOLEDMAINMENU__
#define  __FPPOLEDMAINMENU__


#include "OLEDPages.h"

class FPPStatusOLEDPage;

class FPPMainMenu : public MenuOLEDPage {
public:
    FPPMainMenu(FPPStatusOLEDPage *parent);
    virtual ~FPPMainMenu();
    
    virtual void displaying() override;
    virtual void itemSelected(const std::string &item) override;

private:
    ListOLEDPage *aboutPage;
    
    FPPStatusOLEDPage *statusPage;
};

#endif
