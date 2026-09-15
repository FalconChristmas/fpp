/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include <dirent.h>
#include <filesystem>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CapeUtils.h"

#include <string>

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

int main(int argc, char* argv[]) {
    try {
        bool readonly = false;
        bool noperms = false;
        bool forceDefaults = false;
        bool dryRun = false;
        std::string jurisdiction;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-ro")) {
                readonly = true;
            } else if (!strcmp(argv[i], "-no-set-permissions")) {
                noperms = true;
            } else if (!strcmp(argv[i], "-force-defaults")) {
                forceDefaults = true;
            } else if (!strcmp(argv[i], "-dry-run") || !strcmp(argv[i], "-settings-only")) {
                // Both spellings select the same thing: report what defaultSettings
                // would do and change nothing. -settings-only names the part of a
                // real run that is being simulated; -dry-run names the fact that it
                // is a simulation.
                dryRun = true;
            } else if (!strncmp(argv[i], "-regime=", 8)) {
                jurisdiction = argv[i] + 8;
            }
        }

        // The setup wizard's entry point. Cape detection runs at boot, before
        // anyone has been asked where they are, so a cape's telemetry defaults are
        // held. Once step one of the wizard is answered this reports which of them
        // the answer permits -- without writing anything, because nothing in that
        // page is persisted until the user finishes it.
        if (dryRun) {
            printf("%s\n", CapeUtils::INSTANCE.dryRunSettings(jurisdiction).c_str());
            return 0;
        }

        // Clearing this is part of REGENERATING it, so it must not happen in a
        // read-only run. The wipe used to sit above the argument loop, so
        // `fppcapedetect -ro` deleted the live cape-info.json and then never
        // wrote one back -- loadFiles() only copies the regenerated files out of
        // the scratch directory when !readOnly. A box left in that state has no
        // cape-info.json at all, which MultiSync (:714) and
        // ManageApacheContentPolicy.sh (:92) both read.
        if (!readonly) {
            remove_recursive("/home/fpp/media/tmp/", false);
        }
        CapeUtils::INSTANCE.initCape(readonly, forceDefaults);

        // fppoled keeps a copy of the cape image on the root fs so it can put
        // the logo up before detection has run (it snapshots media/tmp before
        // the wipe above, at ExecStartPre).  Keep that copy in step here too:
        // an EEPROM that is blank or whose header does not match produces no
        // cape-image.xbm at all, and the copy is what OLEDPage::readCapeImage()
        // falls back to in exactly that case -- so without this a re-detect
        // leaves the OLED showing the logo of a cape that is no longer there.
        // Only on a run that regenerated media/tmp: a -ro run copies nothing
        // out, so its "no image" means nothing about the hardware.
        if (!readonly) {
            std::error_code ec;
            if (std::filesystem::is_regular_file("/home/fpp/media/tmp/cape-image.xbm")) {
                std::filesystem::copy_file("/home/fpp/media/tmp/cape-image.xbm",
                                           "/var/tmp/cape-image.xbm",
                                           std::filesystem::copy_options::overwrite_existing, ec);
            } else {
                std::filesystem::remove("/var/tmp/cape-image.xbm", ec);
            }
        }
        if (!noperms) {
            // getpwnam_r() is the thread-safe form of getpwnam().
            char pbuf[16384];
            struct passwd pwd;
            struct passwd* pwres = nullptr;
            if (getpwnam_r("fpp", &pwd, pbuf, sizeof(pbuf), &pwres) == 0 && pwres &&
                std::filesystem::is_directory("/home/fpp/media/tmp/")) {
                for (const auto& entry : std::filesystem::directory_iterator("/home/fpp/media/tmp/")) {
                    chown(entry.path().c_str(), pwres->pw_uid, pwres->pw_gid);
                }
            }
        }
    } catch (std::exception& e) {
        return -1;
    }
    return 0;
}
