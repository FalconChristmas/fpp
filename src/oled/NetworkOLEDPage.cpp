#include "fpp-pch.h"

#include "NetworkOLEDPage.h"

class EditIPOLEDPage : public TitledOLEDPage {
public:
    EditIPOLEDPage(const std::string &type, const std::string &v, FPPNetworkOLEDPage *parent) : TitledOLEDPage(type), networkPage(parent) {
        std::vector<std::string> split = splitWithQuotes(v, '.');
        for (int x = 0; x < 4; x++) {
            std::string val = "000";
            if (x < split.size()) {
                val = split[x];
                while (val.size() < 3) {
                    val = "0" + val;
                }
            }
            ipAs3 += val;
            if (x < 3) {
                ipAs3 += ".";
            }
        }
    }
    virtual ~EditIPOLEDPage() {
    }
    
    void displaying() override {
        if (oledForcedOff) return;
        clearDisplay();
        int startY = displayTitle();

        int xPos = 9 + posIdx * 6;
        
        fillTriangle(xPos, startY + 7, xPos + 3, startY + 3, xPos + 6, startY + 7);
        printString(10, startY + 10, ipAs3, true);
        fillTriangle(xPos, startY + 20, xPos + 3, startY + 24, xPos + 6, startY + 20);

        flushDisplay();
    }
    
    bool doAction(const std::string &action) {
        if (action == "Back" && posIdx == 0) {
            SetCurrentPage(networkPage);
            return true;
        } else if (action == "Back") {
            posIdx--;
            if (posIdx % 4 == 3) {
                posIdx--;
            }
        } else if (action == "Enter") {
            posIdx++;
            if (posIdx >= 15) {
                networkPage->setParameterIP(title, ipAs3);
                SetCurrentPage(networkPage);
                return true;
            }
            if (posIdx % 4 == 3) {
                posIdx++;
            }
        } else if (action == "Up") {
            ipAs3[posIdx]++;
            if (ipAs3[posIdx] > '9') ipAs3[posIdx] = '0';
        } else if (action == "Down" || action == "Test/Down") {
            ipAs3[posIdx]--;
            if (ipAs3[posIdx] < '0') ipAs3[posIdx] = '9';
        }
        displaying();
        return true;
    }
    
    std::string ipAs3;
    int posIdx = 0;
    FPPNetworkOLEDPage *networkPage;
};


FPPNetworkOLEDPage::FPPNetworkOLEDPage(OLEDPage *parent)
: MenuOLEDPage("eth0", {}, parent)
{
    protocol = "dhcp";
    
    char ipc[20];
    char netmaskc[20];
    char gwc[20];
    GetInterfaceAddress("eth0", ipc, netmaskc, gwc);
    
    ip = ipc;
    netmask = netmaskc;
    gateway = gwc;

    if (FileExists("/home/fpp/media/config/interface.eth0")) {
        std::fstream file;
        file.open("/home/fpp/media/config/interface.eth0", std::ios::in);
        
        if (file.is_open()) {
           std::string tp;
           while (getline(file, tp)) {
               std::vector<std::string> v = splitWithQuotes(tp, '=');
               if (v[0] == "PROTO") {
                   protocol = v[1];
               } else if (v[0] == "ADDRESS") {
                   ip = v[1];
               } else if (v[0] == "NETMASK") {
                   netmask = v[1];
               } else if (v[0] == "GATEWAY") {
                   gateway = v[1];
               }
           }
           file.close(); //close the file object.
        }
    }
    if (FileExists("/home/fpp/media/config/dns")) {
        std::fstream file;
        file.open("/home/fpp/media/config/dns", std::ios::in);
        
        if (file.is_open()) {
           std::string tp;
           while (getline(file, tp)) {
               std::vector<std::string> v = splitWithQuotes(tp, '=');
               if (v[0] == "DNS1") {
                   dns1 = v[1];
                   if (dns1 == "\"\"") {
                       dns1 = "";
                   }
               } else if (v[0] == "DNS2") {
                   dns2 = v[1];
                   if (dns2 == "\"\"") {
                       dns2 = "";
                   }
               }
           }
           file.close();
        }
    }
    if (dns1 == "" && FileExists("/etc/resolv.conf")) {
        std::fstream file;
        file.open("/etc/resolv.conf", std::ios::in);
        if (file.is_open()) {
            std::string tp;
            while (getline(file, tp)) {
                std::vector<std::string> v = splitWithQuotes(tp, ' ');
                if (v[0] == "nameserver") {
                    if (dns1 == "") {
                        dns1 = v[1];
                        if (dns1 == "\"\"") {
                            dns1 = "";
                        }
                    } else if (dns2 == "") {
                        dns2 = v[1];
                        if (dns2 == "\"\"") {
                            dns2 = "";
                        }
                    }
                }
            }
        }
       file.close();
    }

    if (protocol == "dhcp") {
        items.push_back("Type: DHCP");
    } else {
        items.push_back("Type: Static");
    }
    items.push_back("IP:" + ip);
    items.push_back("NM:" + netmask);
    items.push_back("GW:" + gateway);
    items.push_back("D1:" + dns1);
    items.push_back("D2:" + dns2);
}

FPPNetworkOLEDPage::~FPPNetworkOLEDPage() {
}

void FPPNetworkOLEDPage::writeSettings() {
    std::fstream file;
    file.open("/home/fpp/media/config/interface.eth0", std::ios::out);
    file << "INTERFACE=eth0\n";
    file << "PROTO=" << protocol << "\n";
    if (protocol == "static") {
        file << "ADDRESS=" << ip << "\n";
        file << "NETMASK=" << netmask << "\n";
        file << "GATEWAY=" << gateway << "\n";
    }
    file.close();
    
    file.open("/home/fpp/media/config/dns", std::ios::out);
    file << "DNS1=\"" << dns1 << "\"\n";
    file << "DNS2=\"" << dns2 << "\"\n";
    file.close();
    
    SetFilePerms("/home/fpp/media/config/interface.eth0");
    SetFilePerms("/home/fpp/media/config/dns");
}



bool FPPNetworkOLEDPage::doAction(const std::string &action) {
    if (protocol == "dhcp" && (action != "Enter" && action != "Back")) {
        return true;
    }
    if (action == "Back" && parent) {
        autoDelete();
    }
    return MenuOLEDPage::doAction(action);
}
void FPPNetworkOLEDPage::setParameterIP(const std::string &tp, const std::string &nipas3) {
    std::vector<std::string> splits = splitWithQuotes(nipas3, '.');
    std::string nip;
    for (int x = 0; x < 4; x++) {
        std::string v = "0";
        if (x < splits.size()) {
            v = splits[x];
            while (v[0] == '0') {
                v = v.substr(1);
            }
            if (v.empty()) {
                v = "0";
            }
        }
        if (x < 3) {
            v += ".";
        }
        nip += v;
    }
    
    if (tp == "IP Address") {
        ip = nip;
        items[1] = "IP:" + ip;
    } else if (tp == "Gateway") {
        gateway = nip;
        items[3] = "GW:" + gateway;
    } else if (tp == "Netmask") {
        netmask = nip;
        items[2] = "NM:" + netmask;
    } else if (tp == "DNS 1") {
        dns1 = nip;
        items[4] = "D1:" + dns1;
    } else if (tp == "DNS 2") {
        dns2 = nip;
        items[5] = "D2:" + dns2;
    }
    writeSettings();
}


void FPPNetworkOLEDPage::itemSelected(const std::string &item) {
    if (item == "Type: Static") {
        protocol = "dhcp";
        writeSettings();
        items[0] = "Type: DHCP";
        curSelected = 0;
        display();
        return;
    } else if (item == "Type: DHCP") {
        protocol = "static";
        writeSettings();
        items[0] = "Type: Static";
        curSelected = 0;
        display();
        return;
    }
    std::vector<std::string> v = splitWithQuotes(item, ':');
    std::string type;
    if (v[0] == "IP") {
        type = "IP Address";
    } else if (v[0] == "GW") {
        type = "Gateway";
    } else if (v[0] == "NM") {
        type = "Netmask";
    } else if (v[0] == "D1") {
        type = "DNS 1";
    } else if (v[0] == "D2") {
        type = "DNS 2";
    }
    std::string val;
    if (v.size() > 1) {
        val = v[1];
    }

    EditIPOLEDPage *p = new EditIPOLEDPage(type, val, this);
    p->autoDelete();
    SetCurrentPage(p);
}
