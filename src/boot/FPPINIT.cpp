/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <systemd/sd-daemon.h>
#include <thread>
#include <unistd.h>

#include "FPPINIT.h"

void teeOutput(const std::string& log) {
    std::string cmd = "tee -a ";
    cmd += log;
    mkdir("/home/fpp/media/", 0775);
    mkdir("/home/fpp/media/logs", 0775);

    FILE* f = popen(cmd.c_str(), "w");
    if (dup2(fileno(f), STDOUT_FILENO) < 0) {
        printf("Couldn't redirect output to log file\n");
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
}

int remove_recursive(const char* const path, bool removeThis = true) {
    DIR* const directory = opendir(path);
    if (directory) {
        struct dirent* entry;
        while ((entry = readdir(directory))) {
            if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
                continue;
            }
            char filename[strlen(path) + strlen(entry->d_name) + 2];
            sprintf(filename, "%s/%s", path, entry->d_name);
            if (entry->d_type == DT_DIR) {
                if (remove_recursive(filename)) {
                    closedir(directory);
                    return -1;
                }
            } else {
                if (remove(filename)) {
                    closedir(directory);
                    return -1;
                }
            }
        }
        if (closedir(directory)) {
            return -1;
        }
    }
    if (removeThis) {
        return remove(path);
    }
    return 0;
}

void exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    struct PipeCloser {
        void operator()(FILE* f) const {
            if (f)
                pclose(f);
        }
    };
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        printf(buffer.data());
    }
}
int execbg(const std::string& cmd) {
    return system(cmd.c_str());
}
std::string execAndReturn(const std::string& cmd) {
    std::array<char, 128> buffer;
    struct PipeCloser {
        void operator()(FILE* f) const {
            if (f)
                pclose(f);
        }
    };
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    std::string ret;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        ret += std::string(buffer.data());
    }
    return ret;
}

bool LoadJsonFromString(const std::string& str, Json::Value& root) {
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    builder["collectComments"] = false;
    bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &errors);
    delete reader;
    if (!success) {
        Json::Value empty;
        root = empty;
        return false;
    }
    return true;
}
std::string SaveJsonToString(const Json::Value& root) {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "\t";

    std::string result = Json::writeString(wbuilder, root);

    return result;
}

void modprobe(const char* mod) {
    std::string cmd = std::string("/usr/sbin/modprobe ") + mod;
    exec(cmd);
}

int main(int argc, char* argv[]) {
    std::string networkSetupMut = FPP_MEDIA_DIR + "/tmp/networkSetup";
    std::string action = "start";
    if (argc > 1) {
        action = argv[1];
    }
    FILE* f = fopen((FPP_MEDIA_DIR + "/logs/fpp_init.log").c_str(), "a+");
    int sleepCount = 0;
    while (!f && sleepCount < 30) {
        printf("Could not open log\n");
        sleepCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        f = fopen((FPP_MEDIA_DIR + "/logs/fpp_init.log").c_str(), "a+");
    }
    if (f) {
        fclose(f);
        if (action == "start") {
            teeOutput(FPP_MEDIA_DIR + "/logs/fpp_init.log");
        } else if (action == "postNetwork") {
            teeOutput(FPP_MEDIA_DIR + "/logs/fpp_boot.log");
        }
    } else {
        printf("Could not too output to log.  Directory might not be writable or maybe full.\n");
    }
    printf("------------------------------\nRunning FPPINIT %s", action.c_str());
    for (int x = 2; x < argc; x++) {
        printf(" %s", argv[x]);
    }
    printf("\n");
    if (action == "start") {
        if (!FileExists("/etc/fpp/container")) {
#ifdef CAPEDETECT
            printf("FPP - Cleaning tmp\n");
            remove_recursive("/home/fpp/media/tmp/", false);
#endif
        }
        createDirectories();
        printf("FPP - Directories created\n");
        bool needReboot = checkUnpartitionedSpace();
        checkSSHKeys();
        handleBootPartition();
        checkHostName();
        checkFSTAB();
        setupApache();
        DetectCape();
        setupTimezone();
        int reboot = getRawSettingInt("rebootFlag", 0);
        if (reboot && !needReboot) {
            printf("FPP - Clearing reboot flags\n");
            setRawSetting("rebootFlag", "0");
        }
        handleBootActions();
        runScripts("boot", true);
        int skipNetwork = FileExists("/etc/fpp/desktop") || getRawSettingInt("SkipNetworkReset", 0);
        if (!skipNetwork) {
            setupNetwork();
        }
        configureBBB();
        setupChannelOutputs();
        setupKiosk();
        printf("Setting file ownership\n");
        setFileOwnership();
        PutFileContents(FPP_MEDIA_DIR + "/tmp/cape_detect_done", "1");
        checkInstallKiosk();

        if (!FileExists("/etc/fpp/container")) {
            // Create boot delay flag file early if boot delay is configured
            // so UI can show warning immediately when Apache starts
            int bootDelaySetting = getRawSettingInt("bootDelay", -1);
            if (bootDelaySetting != 0) {
                // Store start time and duration/mode for UI countdown
                time_t startTime = time(nullptr);
                if (bootDelaySetting > 0) {
                    std::string flagContent = std::to_string(startTime) + "," + std::to_string(bootDelaySetting);
                    PutFileContents(FPP_MEDIA_DIR + "/tmp/boot_delay", flagContent);
                } else if (bootDelaySetting == -1) {
                    std::string flagContent = std::to_string(startTime) + ",auto";
                    PutFileContents(FPP_MEDIA_DIR + "/tmp/boot_delay", flagContent);
                }
            }
        } else {
            // Ensure no boot delay flag file exists. No delay in docker.
            unlink((FPP_MEDIA_DIR + "/tmp/boot_delay").c_str());
        }

        // Notify systemd that initialization is complete
        sd_notify(0, "READY=1\nSTATUS=FPP initialization complete");
    } else if (action == "postNetwork") {
        handleBootDelay();
        // turn off blinking cursor + clean up kiosk leftovers (quick, independent)
        PutFileContents("/sys/class/graphics/fbcon/cursor_blink", "0");
        cleanupChromiumFiles();

        // Audio setup and network bring-up no longer depend on each other (the
        // flite IP announcement moved out to fpp-announce-ip.service), so run
        // them concurrently. The network thread spends most of its time *waiting*
        // (DHCP lease, NTP sync), yielding the CPU, while setupAudio does its
        // CPU/IO work -- so they overlap well even on a single-core board and
        // shorten postNetwork.
        //
        // Thread-safety: the only shared mutable state is /home/fpp/media/settings.
        // setupAudio() is the SOLE writer; the network functions only read it.
        // get/setRawSetting go through GetFileContents/PutFileContents, which take
        // an advisory flock (LOCK_SH / LOCK_EX), so a read never observes a partial
        // write. Keep the network path settings-read-only to preserve this. Each
        // thread is wrapped so an exception can't escape and std::terminate the
        // whole boot.
        std::thread audioThread([]() {
            try {
                setupAudio();
                // The PipeWire services ship disabled and are NOT started at
                // sound.target -- starting them only now, after setupAudio has
                // written/validated the config, means PipeWire reads the correct
                // graph on its first (and only) start instead of starting empty
                // at boot and needing a restart. Backgrounded so the audio thread
                // doesn't block on the service bring-up. fpp-pipewire is pulled in
                // by the other two's Requires=, but list it for clarity.
                execbg("systemctl start fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service &");
            } catch (const std::exception& e) {
                printf("FPP - setupAudio failed: %s\n", e.what());
            }
        });
        std::thread networkThread([]() {
            try {
                removeDummyInterface();
                checkWLANInterface();
                removeDummyInterface();
                waitForInterfacesUp(100); // wait for an IP (needed for the time-sync wait)
                // Time sync wait happens AFTER interfaces are up so NTP can sync
                handleTimeSyncWait();
                if (!FileExists("/etc/fpp/desktop")) {
                    maybeEnableTethering();
                    detectNetworkModules();
                }
            } catch (const std::exception& e) {
                printf("FPP - network setup failed: %s\n", e.what());
            }
        });
        // setupTimezone and swap setup depend on neither thread, so run them here
        // -- concurrent with the network thread's DHCP/NTP wait and the audio
        // thread -- instead of serially after the joins. setupTimezone only READS
        // settings, which is flock-safe against setupAudio's writes.
        setupTimezone(); // this may not have worked in the init phase, try again
        startZRAMSwap();
        startDiskSwap();

        audioThread.join();
        networkThread.join();

        // Both of these must run after a join:
        //  - checkInstallPackages may apt-install user packages after an OS
        //    upgrade, so it needs the network up (network thread joined).
        //  - setFileOwnership chowns /home/fpp/media, where setupAudio writes its
        //    PipeWire config as root, so it must follow the audio thread -- run it
        //    last so it also catches anything checkInstallPackages dropped there.
        checkInstallPackages();
        setFileOwnership();
        // Notify systemd that post-network setup is complete
        sd_notify(0, "READY=1\nSTATUS=FPP post-network setup complete");
    } else if (action == "bootPre") {
        int restart = getRawSettingInt("restartFlag", 0);
        if (restart) {
            printf("FPP - Clearing restart flags\n");
            setRawSetting("restartFlag", "0");
        }
        setupChannelOutputs();
        runScripts("preStart", true);
    } else if (action == "bootPost") {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        runScripts("postStart", false);
    } else if (action == "runPreStartScripts") {
        runScripts("preStart", true);
    } else if (action == "runPostStartScripts") {
        runScripts("postStart", false);
    } else if (action == "runPreStopScripts") {
        runScripts("preStop", true);
    } else if (action == "runPostStopScripts") {
        runScripts("postStop", false);
    } else if (action == "setupAudio") {
        setupAudio();
    } else if (action == "announceIP") {
        // Run from fpp-announce-ip.service, after fpp_postnetwork + PipeWire, so
        // the flite synthesis doesn't slow the tail of postNetwork / fppd start.
        announceIPAddresses();
    } else if (action == "configureBBB") {
        configureBBB();
    } else if (action == "installKiosk") {
        installKiosk();
    } else if (action == "setupNetwork") {
        PutFileContents(networkSetupMut, "1");
        setupNetwork(true);
        waitForInterfacesUp(70);
        detectNetworkModules();
        maybeEnableTethering();
        unlink(networkSetupMut.c_str());
    } else if (action == "checkForTether") {
        if (!FileExists(networkSetupMut)) {
            std::string a = execAndReturn("systemctl is-active fpp_postnetwork");
            TrimWhiteSpace(a);
            if (a.starts_with("active") && argc >= 3) {
                std::string iface = argv[2];
                if (iface.starts_with("wlan")) {
                    waitForInterfacesUp(20);
                    maybeEnableTethering();
                    detectNetworkModules();
                }
            }
        }
    } else if (action == "maybeRemoveTether") {
        int te = getRawSettingInt("EnableTethering", 0);
        if (!FileExists(networkSetupMut) && (te != 1)) {
            std::string e = execAndReturn("systemctl is-enabled hostapd");
            std::string a = execAndReturn("systemctl is-active hostapd");
            TrimWhiteSpace(e);
            TrimWhiteSpace(a);
            if (a.starts_with("active") || e.starts_with("enabled")) {
                std::string iface = argv[2];
                if (FindTetherWIFIAdapater() != iface && !iface.starts_with("usb") && !iface.starts_with("lo") && waitForInterfacesUp(10)) {
                    exec("rm -f /etc/systemd/network/10-" + FindTetherWIFIAdapater() + ".network");
                    exec("rm -f /home/fpp/media/tmp/wifi-*.ascii");
                    exec("systemctl stop hostapd.service");
                    exec("systemctl disable hostapd.service");
                    exec("systemctl reload-or-restart systemd-networkd.service");
                }
            }
        }
    } else if (action == "detectNetworkModules") {
        detectNetworkModules();
    } else if (action == "reboot") {
        handleRebootActions();
    } else if (action == "resizeRootFS") {
        resizeRootFS();
    }
    printf("------------------------------\n");
    return 0;
}
