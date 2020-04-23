#pragma once

#include "OLEDPages.h"


class FPPNetworkOLEDPage : public MenuOLEDPage {
public:
    FPPNetworkOLEDPage(OLEDPage *parent);
    virtual ~FPPNetworkOLEDPage();
    
    virtual bool doAction(const std::string &action) override;
    virtual void itemSelected(const std::string &item) override;

    void writeSettings();
    void setParameterIP(const std::string &tp, const std::string &ip);
    std::string protocol;
    std::string ip;
    std::string netmask;
    std::string gateway;
    std::string dns1;
    std::string dns2;
};

