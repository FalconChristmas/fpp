
#include <unistd.h>
#include <cstdlib>

#include "FPPMainMenu.h"
#include "../fppversion.h"
#include "SSD1306_OLED.h"

#include "common.h"
#include "FPPStatusOLEDPage.h"
#include "settings.h"
#include <jsoncpp/json/json.h>


static int curlBufferWriter(char *data, size_t size, size_t nmemb,
                            std::string *writerData) {
    if (writerData == NULL) {
        return 0;
    }
    writerData->append(data, size * nmemb);
    return size * nmemb;
}

static std::string doCurlGet(const std::string &url, int timeout = 250) {
    const std::string buffer;
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlBufferWriter);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return buffer;
}

class RebootPromptPage : public PromptOLEDPage {
public:
    RebootPromptPage(OLEDPage *p)
        : PromptOLEDPage("Reboot?", "Reboot FPPD?", "", {"OK", "Cancel"}), parent(p) {}
    RebootPromptPage(const std::string &msg1, const std::string &msg2, OLEDPage *p)
        : PromptOLEDPage("Reboot?", msg1, msg2, {"OK", "Cancel"}), parent(p) {}
    virtual ~RebootPromptPage() {};
    
    
    virtual void ItemSelected(const std::string &item) override {
        if (item == "Cancel") {
            SetCurrentPage(parent);
        } else {
            SetCurrentPage(nullptr);
            clearDisplay();
            setTextSize(1);
            setTextColor(WHITE);
            setCursor(4, 17);
            print_str("Rebooting...");
            Display();
            sync();
            system("/sbin/reboot");
        }
    }

private:
    OLEDPage *parent;
};

class ShutdownPromptPage : public PromptOLEDPage {
    public:
    ShutdownPromptPage(OLEDPage *p)
    : PromptOLEDPage("Shutdown?", "Shutdown FPPD?", "", {"OK", "Cancel"}), parent(p) {}
    ShutdownPromptPage(const std::string &msg1, const std::string &msg2, OLEDPage *p)
    : PromptOLEDPage("Shutdown?", msg1, msg2, {"OK", "Cancel"}), parent(p) {}
    virtual ~ShutdownPromptPage() {};
    
    
    virtual void ItemSelected(const std::string &item) override {
        if (item == "Cancel") {
            SetCurrentPage(parent);
        } else {
            SetCurrentPage(nullptr);
            clearDisplay();
            setTextSize(1);
            setTextColor(WHITE);
            setCursor(4, 17);
            print_str("Shutdown...");
            Display();
            sync();
            system("/sbin/shutdown -h now");
        }
    }
    
    private:
    OLEDPage *parent;
};

class BridgeStatsPage : public ListOLEDPage {
public:
    BridgeStatsPage(OLEDPage *parent) : ListOLEDPage("Bridge Stats", {}, parent) {};
    virtual ~BridgeStatsPage() {}
    
    
    virtual void displaying() override {
        bool on = true;
        doIteration(on);
    }
    virtual bool doIteration(bool &displayOn) override {
        std::string d = doCurlGet("http://localhost:32322/fppd/e131stats", 10000);
        Json::Value result;
        if (LoadJsonFromString(d, result)) {
            items.clear();
            items.push_back("Univ    Pckts  Err");
            for (int x = 0; x < result["universes"].size(); x++) {
                Json::Value u = result["universes"][x];
                std::string l = u["id"].asString();
                l += ":";
                if (l.length() < 8) {
                    l.append(8 - l.length(), ' ');
                }
                l += u["packetsReceived"].asString();
                if (l.length() < 15) {
                    l.append(15 - l.length(), ' ');
                }
                l += u["errors"].asString();
                items.push_back(l);
            }
            display();
        }
        return false;
    }
private:
};

FPPMainMenu::FPPMainMenu(FPPStatusOLEDPage *p)
: MenuOLEDPage("Main Menu", {"FPP Mode", "Tethering", "Testing", "Reboot", "Shutdown", "About", "Back"}, p),
  aboutPage(nullptr), statusPage(p) {
    
}
FPPMainMenu::~FPPMainMenu() {
    if (aboutPage) {
        delete aboutPage;
    }
}


void FPPMainMenu::displaying() {
    std::string mode = statusPage->getCurrentMode();
    if (mode == "Bridge") {
        items = {"Bridge Stats", "FPP Mode", "Tethering", "Testing", "Reboot", "Shutdown", "About", "Back"};
    } else if (mode != "Remote") {
        items = {"Start Playlist", "FPP Mode", "Tethering", "Testing", "Reboot", "Shutdown", "About", "Back"};
    }
    MenuOLEDPage::displaying();
}

void FPPMainMenu::itemSelected(const std::string &item) {
    if (item == "Back") {
        SetCurrentPage(parent);
    } else if (item == "About") {
        if (aboutPage == nullptr) {
            std::string ver = "Version: ";
            ver += getFPPMajorVersion();
            ver += ".";
            ver += getFPPMinorVersion();
            std::string branch = "Branch: ";
            branch += getFPPBranch();
            aboutPage = new ListOLEDPage("About FPP", {ver, branch}, this);
        }
        SetCurrentPage(aboutPage);
    } else if (item == "Reboot") {
        RebootPromptPage *pg = new RebootPromptPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Shutdown") {
        ShutdownPromptPage *pg = new ShutdownPromptPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Testing") {
        FPPStatusOLEDPage *sp = statusPage;
        MenuOLEDPage *pg = new MenuOLEDPage("Test", {
            "Off",
            "White", "Red", "Green", "Blue",
            "R-G-B Cycle", "R-G-B-W Cycle", "R-G-B-W-N Cycle",
            "R-G-B Chase", "R-G-B-W Chase", "R-G-B-W-N Chase",
            "Back"
        }, [this, sp] (const std::string &item) {
            if (item == "Back") {
                SetCurrentPage(this);
                return;
            }
            sp->runTest(item);
            if (item == "Off") {
                SetCurrentPage(this);
                return;
            }
        }, this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Tethering") {
        FPPStatusOLEDPage *sp = statusPage;
        std::vector<std::string> options = {
            " Automatic",
            " On",
            " Off",
            " Back"
        };
        int tm = getSettingInt("EnableTethering");
        options[tm][0] = '*';
        MenuOLEDPage *pg = new MenuOLEDPage("Tethering", options, [this, sp] (const std::string &item) {
            if (item != " Back") {
                std::string nitem = item.substr(1);
                std::string nv = "http://localhost/fppjson.php?command=setSetting&plugin=&key=EnableTethering&value=";
                if (nitem == "Automatic") {
                    nv += "0";
                } else if (nitem == "On") {
                    nv += "1";
                } else if (nitem == "Off") {
                    nv += "2";
                } else {
                    nv += "0";
                }
                doCurlGet(nv);
                doCurlGet("http://localhost/fppjson.php?command=setSetting&key=rebootFlag&value=1");
                
                
                RebootPromptPage *pg = new RebootPromptPage("Reboot Required", "Reboot FPPD?", this);
                pg->autoDelete();
                SetCurrentPage(pg);
            } else {
                SetCurrentPage(this);
            }
        }, this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "FPP Mode") {
        FPPStatusOLEDPage *sp = statusPage;
        std::vector<std::string> options = {
            " Standalone",
            " Master",
            " Remote",
            " Bridge",
            " Back"
        };
        std::string mode = statusPage->getCurrentMode();
        if (mode == "Bridge") {
            options[3][0] = '*';
        } else if (mode == "Remote") {
            options[2][0] = '*';
        } else if (mode == "Master") {
            options[1][0] = '*';
        } else {
            options[0][0] = '*';
        }
        MenuOLEDPage *pg = new MenuOLEDPage("FPPD Mode", options, [this, sp] (const std::string &item) {
            if (item != " Back") {
                std::string nitem = item.substr(1);
                std::string nv = "http://localhost/fppxml.php?command=setFPPDmode&mode=";
                if (nitem == "Bridge") {
                    nv += "1";
                } else if (nitem == "Master") {
                    nv += "6";
                } else if (nitem == "Remote") {
                    nv += "8";
                } else {
                    nv += "2";
                }
                doCurlGet(nv);
                doCurlGet("http://localhost/fppxml.php?command=restartFPPD", 10000);
                SetCurrentPage(sp);
            } else {
                SetCurrentPage(this);
            }
        }, this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Bridge Stats") {
        BridgeStatsPage *pg = new BridgeStatsPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Start Playlist") {
        std::string d = doCurlGet("http://localhost/api/playlists", 10000);
        Json::Value result;
        if (LoadJsonFromString(d, result)) {
            std::vector<std::string> playlists;
            playlists.push_back("-Stop Now-");
            playlists.push_back("-Stop Gracefully-");
            for (int x = 0; x < result.size(); x++) {
                playlists.push_back(result[x].asString());
            }
            playlists.push_back("Back");
            FPPStatusOLEDPage *sp = statusPage;
            MenuOLEDPage *pg = new MenuOLEDPage("Playlist", playlists, [sp] (const std::string &item) {
                if (item == "-Stop Now-") {
                    std::string url = "http://localhost/fppxml.php?command=stopNow";
                    doCurlGet(url, 1000);
                } else if (item == "-Stop Gracefully-") {
                    std::string url = "http://localhost/fppxml.php?command=stopGracefully";
                    doCurlGet(url, 1000);
                } else if (item != "Back") {
                    std::string url = "http://localhost/fppxml.php?command=startPlaylist&repeat=checked&playEntry=0&section=&playList=";
                    url += item;
                    doCurlGet(url, 1000);
                }
                SetCurrentPage(sp);
            }, this);
            pg->autoDelete();
            SetCurrentPage(pg);
        }
    }
}
