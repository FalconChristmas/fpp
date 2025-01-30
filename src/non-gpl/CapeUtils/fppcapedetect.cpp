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
#include <thread>
#include <unistd.h>

#include "CapeUtils.h"

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
    remove_recursive("/home/fpp/media/tmp/", false);
    try {
        bool readonly = false;
        bool noperms = false;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-ro")) {
                readonly = true;
            } else if (!strcmp(argv[i], "-no-set-permissions")) {
                noperms = true;
            }
        }
        CapeUtils::INSTANCE.initCape(readonly);
        if (!noperms) {
            struct passwd* pwd = getpwnam("fpp");
            if (pwd) {
                for (const auto& entry : std::filesystem::directory_iterator("/home/fpp/media/tmp/")) {
                    chown(entry.path().c_str(), pwd->pw_uid, pwd->pw_gid);
                }
            }
        }
    } catch (std::exception& e) {
        return -1;
    }
    return 0;
}
