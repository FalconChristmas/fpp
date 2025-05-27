#include "fpp-pch.h"

#include <net/if.h>
#include <unistd.h>

#include "FPPMainMenu.h"
#include "../Warnings.h"
#include "../common.h"
#include "../fppversion.h"
#include "../log.h"
#include "../settings.h"

#include "FPPStatusOLEDPage.h"
#include "NetworkOLEDPage.h"

static int curlBufferWriter(char* data, size_t size, size_t nmemb,
                            std::string* writerData) {
    if (writerData == NULL) {
        return 0;
    }
    writerData->append(data, size * nmemb);
    return size * nmemb;
}

static std::string escapeURL(const std::string& url) {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    char* output = curl_easy_escape(curl, url.c_str(), url.length());
    std::string ret = output;
    curl_free(output);
    curl_easy_cleanup(curl);
    return ret;
}

static std::string doCurlGet(const std::string& url, int timeout = 250) {
    const std::string buffer;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
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
    RebootPromptPage(OLEDPage* p) :
        PromptOLEDPage("Reboot?", "Reboot FPPD?", "", { "OK", "Cancel" }),
        parent(p) {}
    RebootPromptPage(const std::string& msg1, const std::string& msg2, OLEDPage* p) :
        PromptOLEDPage("Reboot?", msg1, msg2, { "OK", "Cancel" }),
        parent(p) {}
    virtual ~RebootPromptPage() {};

    virtual void ItemSelected(const std::string& item) override {
        if (item == "Cancel") {
            SetCurrentPage(parent);
        } else {
            SetCurrentPage(nullptr);
            clearDisplay();
            printString(4, 17, "Rebooting...");
            flushDisplay();
            sync();
            system("/sbin/reboot");
        }
    }

private:
    OLEDPage* parent;
};

class ShutdownPromptPage : public PromptOLEDPage {
public:
    ShutdownPromptPage(OLEDPage* p) :
        PromptOLEDPage("Shutdown?", "Shutdown FPPD?", "", { "OK", "Cancel" }),
        parent(p) {}
    ShutdownPromptPage(const std::string& msg1, const std::string& msg2, OLEDPage* p) :
        PromptOLEDPage("Shutdown?", msg1, msg2, { "OK", "Cancel" }),
        parent(p) {}
    virtual ~ShutdownPromptPage() {};

    virtual void ItemSelected(const std::string& item) override {
        if (item == "Cancel") {
            SetCurrentPage(parent);
        } else {
            SetCurrentPage(nullptr);
            clearDisplay();
            printString(4, 17, "Shutdown...");
            flushDisplay();
            sync();
            system("/sbin/shutdown -h now");
        }
    }

private:
    OLEDPage* parent;
};

class BridgeStatsPage : public ListOLEDPage {
public:
    BridgeStatsPage(OLEDPage* parent) :
        ListOLEDPage("Bridge Stats", {}, parent) {};
    virtual ~BridgeStatsPage() {}

    virtual void displaying() override {
        bool on = true;
        doIteration(on);
    }
    virtual bool doIteration(bool& displayOn) override {
        std::string d = doCurlGet("http://127.0.0.1:32322/fppd/e131stats", 10000);
        Json::Value result;
        if (LoadJsonFromString(d, result)) {
            items.clear();
            items.emplace_back("Univ    Pckts  Err");
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

class ResetPage : public MenuOLEDPage {
public:
    ResetPage(OLEDPage* parent) :
        MenuOLEDPage("Reset FPP", { "Perform Reset", "Back", "-------------", "All", "  Networking", "  Configuration", "  Outputs", "  Settings", "  Sequences", "  Media", "  Playlists", "  Schedule" }, parent) {
        this->autoDelete();
    };
    virtual ~ResetPage() {}

    virtual void itemSelected(const std::string& item) override {
        if (item == "Back") {
            SetCurrentPage(parent);
            return;
        } else if (item == "Perform Reset") {
            PromptOLEDPage* p = new PromptOLEDPage("Reset FPP?", "Reset FPP?", "", { "No", "Yes" }, [parent = this->parent, it = this->items](const std::string& i) {
                printf("Item: %s\n", i.c_str());
                if (i == "No") {
                    SetCurrentPage(parent);
                } else if (i == "Yes") {
                    performReset(parent, it);
                }
            });
            p->autoDelete();
            SetCurrentPage(p);
        } else if (item == "All") {
            for (int x = 4; x < items.size(); x++) {
                items[x][0] = '*';
            }
            display();
        } else if (item[0] != '-') {
            for (auto& a : items) {
                if (a == item) {
                    a[0] = item[0] == ' ' ? '*' : ' ';
                }
            }
            display();
        }
    }

    static void performReset(OLEDPage* parent, const std::vector<std::string>& items) {
        std::string areas;
        printf("Performing reset\n");
        for (int x = 4; x < items.size(); x++) {
            if (items[x][0] == '*') {
                std::string i = items[x].substr(2);
                if (i == "Networking") {
                    areas += "networking,";
                } else if (i == "Configuration") {
                    areas += "config,";
                } else if (i == "Outputs") {
                    areas += "channeloutputs,";
                } else if (i == "Settings") {
                    areas += "settings,";
                } else if (i == "Sequences") {
                    areas += "sequences,";
                } else if (i == "Media") {
                    areas += "media,";
                } else if (i == "Playlists") {
                    areas += "playlists,";
                } else if (i == "Schedule") {
                    areas += "schedule,";
                }
            }
        }
        if (!areas.empty()) {
            areas = areas.substr(0, areas.length() - 1);
            printf("Areas: %s\n", areas.c_str());
            std::string url = "http://127.0.0.1/resetConfig.php?areas=" + areas;
            printf("Reset URL: %s\n", url.c_str());
            std::string d = doCurlGet(url, 10000);
            printf("%s\n", d.c_str());
        }
        SetCurrentPage(parent);
    }
};

FPPMainMenu::FPPMainMenu(FPPStatusOLEDPage* p) :
    MenuOLEDPage("Main Menu", { "FPP Mode", "Tethering", "Testing", "Reboot", "Shutdown", "Reset", "About", "Back" }, p),
    aboutPage(nullptr),
    statusPage(p) {
}
FPPMainMenu::~FPPMainMenu() {
    if (aboutPage) {
        delete aboutPage;
    }
}

void FPPMainMenu::displaying() {
    std::string mode = statusPage->getCurrentMode();
    items.clear();
    if (mode == "Bridge") {
        items.push_back("Bridge Stats");
    } else if (mode != "Remote") {
        items.push_back("Start Playlist");
    }
    items.push_back("FPP Mode");
    if (Has4DirectionControls()) {
        bool hasEth0 = false;
        struct if_nameindex *if_nidxs, *intf;

        if_nidxs = if_nameindex();
        if (if_nidxs != NULL) {
            for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++) {
                std::string n = intf->if_name;
                if (n == "eth0") {
                    hasEth0 = true;
                }
            }
            if_freenameindex(if_nidxs);
        }
        if (hasEth0) {
            items.push_back("Network");
        }
    }
    std::vector<std::string> it = { "Tethering", "Testing", "Reboot", "Shutdown", "Reset", "About", "Back" };
    items.insert(std::end(items), std::begin(it), std::end(it));

    MenuOLEDPage::displaying();
}

static std::vector<std::string> TEST_OPTIONS = {
    " Enable Multisync", "Off", "White", "Red", "Green", "Blue", "R-G-B Cycle", "R-G-B-W Cycle", "R-G-B-W-N Cycle", "R-G-B Chase", "R-G-B-W Chase", "R-G-B-W-N Chase", "Back"
};

class TestingMenuOLEDPage : public MenuOLEDPage {
public:
    TestingMenuOLEDPage(FPPStatusOLEDPage* sp, OLEDPage* parent) :
        MenuOLEDPage("Test", TEST_OPTIONS, parent), statusPage(sp) {
        if (sp->isMultiSyncTest()) {
            items[0][0] = '*';
        }
    }

    virtual void itemSelected(const std::string& item) override {
        // printf("Item: %s\n", item.c_str());
        if (item == " Enable Multisync") {
            items[0][0] = '*';
            statusPage->setMultiSyncTest(true);
            display();
            return;
        } else if (item == "*Enable Multisync") {
            items[0][0] = ' ';
            statusPage->setMultiSyncTest(false);
            display();
            return;
        } else if (item == "Back") {
            SetCurrentPage(parent);
            return;
        }
        statusPage->runTest(item, statusPage->isMultiSyncTest());
        if (item == "Off") {
            SetCurrentPage(statusPage);
            return;
        }
    }

    FPPStatusOLEDPage* statusPage;
};

void FPPMainMenu::itemSelected(const std::string& item) {
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
            aboutPage = new ListOLEDPage("About FPP", { ver, branch }, this);
        }
        SetCurrentPage(aboutPage);
    } else if (item == "Reboot") {
        RebootPromptPage* pg = new RebootPromptPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Shutdown") {
        ShutdownPromptPage* pg = new ShutdownPromptPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Network") {
        FPPNetworkOLEDPage* pg = new FPPNetworkOLEDPage(this);
        SetCurrentPage(pg);
    } else if (item == "Reset") {
        ResetPage* pg = new ResetPage(this);
        SetCurrentPage(pg);
    } else if (item == "Testing") {
        FPPStatusOLEDPage* sp = statusPage;
        TestingMenuOLEDPage* pg = new TestingMenuOLEDPage(sp, this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Tethering") {
        FPPStatusOLEDPage* sp = statusPage;
        std::vector<std::string> options = {
            " Automatic",
            " On",
            " Off",
            " Back"
        };
        int tm = getSettingInt("EnableTethering");
        options[tm][0] = '*';
        MenuOLEDPage* pg = new MenuOLEDPage(
            "Tethering", options, [this, sp](const std::string& item) {
                if (item != " Back") {
                    std::string nitem = item.substr(1);
                    std::string nvdata;
                    std::string nvresults;
                    std::string nv = "http://127.0.0.1/api/settings/EnableTethering";
                    if (nitem == "Automatic") {
                        nvdata = std::string("0");
                    } else if (nitem == "On") {
                        nvdata = std::string("1");
                    } else if (nitem == "Off") {
                        nvdata = std::string("2");
                    } else {
                        nvdata = std::string("0");
                    }
                    urlPut(nv, nvdata, nvresults);
                    nvdata = std::string("1");
                    urlPut("http://127.0.0.1/api/settings/rebootFlag", "1", nvresults);

                    RebootPromptPage* pg = new RebootPromptPage("Reboot Required", "Reboot FPPD?", this);
                    pg->autoDelete();
                    SetCurrentPage(pg);
                } else {
                    SetCurrentPage(this);
                }
            },
            this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "FPP Mode") {
        FPPStatusOLEDPage* sp = statusPage;
        std::vector<std::string> options = {
            " Player",
            " Remote",
            " Back"
        };
        std::string mode = statusPage->getCurrentMode();
        if (mode == "Remote") {
            options[1][0] = '*';
        } else {
            options[0][0] = '*';
        }
        MenuOLEDPage* pg = new MenuOLEDPage(
            "FPPD Mode", options, [this, sp](const std::string& item) {
                if (item != " Back") {
                    std::string nitem = item.substr(1);
                    std::string nv = "player";
                    if (nitem == "Remote") {
                        nv = "remote";
                    }
                    std::string nvresults;
                    urlPut("http://127.0.0.1/api/settings/fppMode", nv, nvresults);
                    doCurlGet("http://127.0.0.1/api/system/fppd/restart", 10000);
                    SetCurrentPage(sp);
                } else {
                    SetCurrentPage(this);
                }
            },
            this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Bridge Stats") {
        BridgeStatsPage* pg = new BridgeStatsPage(this);
        pg->autoDelete();
        SetCurrentPage(pg);
    } else if (item == "Start Playlist") {
        std::string d = doCurlGet("http://127.0.0.1/api/playlists/playable", 10000);
        Json::Value result;
        if (LoadJsonFromString(d, result)) {
            std::vector<std::string> playlists;
            playlists.push_back("-Stop Now-");
            playlists.push_back("-Stop Gracefully-");
            for (int x = 0; x < result.size(); x++) {
                playlists.push_back(result[x].asString());
            }
            playlists.push_back("Back");
            FPPStatusOLEDPage* sp = statusPage;
            MenuOLEDPage* pg = new MenuOLEDPage(
                "Playlist", playlists, [sp](const std::string& item) {
                    if (item == "-Stop Now-") {
                        std::string url = "http://127.0.0.1/api/playlists/stop";
                        doCurlGet(url, 1000);
                    } else if (item == "-Stop Gracefully-") {
                        std::string url = "http://127.0.0.1/api/playlists/stopgracefully";
                        doCurlGet(url, 1000);
                    } else if (item != "Back") {
                        std::string escItem = escapeURL(item);
                        std::string url = "http://127.0.0.1/api/playlist/" + escItem + "/start";
                        doCurlGet(url, 1000);
                    }
                    SetCurrentPage(sp);
                },
                this);
            pg->autoDelete();
            SetCurrentPage(pg);
        }
    }
}
