/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#if __has_include(<kms++/kms++.h>)
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#define HAS_KMS_FB
#endif

#include "common.h"
#include "fppversion.h"
#include "settings.h"
#include "commands/Commands.h"

#include "fpp.h"

int socket_fd;
struct sockaddr_un server_address;
struct sockaddr_un client_address;
int bytes_received, bytes_sent;

char command[8192];
char response[256];

void SendCommand(const char* com);
void SetupDomainSocket(void);
void Usage(char* appname);

socklen_t address_length;

void GetFrameBufferDevices(Json::Value& v, bool debug) {
    std::string devString = getSetting("framebufferControlSocketPath", "/dev") + "/";
    for (int x = 0; x < 10; x++) {
        std::string fb = "fb";
        fb += std::to_string(x);
        if (FileExists(devString + fb)) {
            if (debug) {
                Json::Value v2(Json::objectValue);
                v[fb] = v2;
            } else {
                v.append(fb);
            }
        }
    }
#ifdef HAS_KMS_FB
    for (int cn = 0; cn < 10; cn++) {
        if (FileExists("/dev/dri/card" + std::to_string(cn))) {
            kms::Card card("/dev/dri/card" + std::to_string(cn));
            for (auto& c : card.get_connectors()) {
                std::string cname = c->fullname();
                if (debug) {
                    std::set<std::string> formats;
                    if (c->get_current_crtc()) {
                        for (auto& f : c->get_current_crtc()->get_primary_plane()->get_formats()) {
                            formats.insert(PixelFormatToFourCC(f));
                        }
                    }
                    Json::Value v2(Json::objectValue);
                    for (auto m : c->get_modes()) {
                        v2["modes"].append(m.to_string_short());
                    }
                    for (auto& a : formats) {
                        v2["formats"].append(a);
                    }
                    v[cname] = v2;
                } else {
                    v.append(cname);
                }
            }
        }
    }
#endif
#ifdef PLATFORM_OSX
    if (v.size() == 0) {
        if (debug) {
            v["fb0"] = Json::Value(Json::objectValue);
            v["fb1"] = Json::Value(Json::objectValue);
            v["fb2"] = Json::Value(Json::objectValue);
        } else {
            v.append("fb0");
            v.append("fb1");
            v.append("fb2");
        }
    }
#endif
}

int main(int argc, char* argv[]) {
    LoadSettings(argv[0]);
    memset(command, 0, sizeof(command));
    SetupDomainSocket();
    if (argc > 1) {
        // Shutdown fppd daemon used by scripts/fppd_stop
        if (strncmp(argv[1], "-q", 2) == 0) {
            snprintf(command, sizeof(command), "q");
            SendCommand(command);
        }
        // Reload schedule example "fpp -R"
        else if (strncmp(argv[1], "-R", 2) == 0) {
            snprintf(command, sizeof(command), "R");
            SendCommand(command);
        }
        // Set new log level - "fpp --log-level info"   "fpp --log-level debug"
        else if ((strcmp(argv[1], "--log-level") == 0) && argc > 2) {
            snprintf(command, sizeof(command), "LogLevel,%s,", argv[2]);
            SendCommand(command);
        }
        // Set new log mask - "fpp --log-mask channel,mediaout"   "fpp --log-mask all"
        else if ((strcmp(argv[1], "--log-mask") == 0) && argc > 2) {
            char newMask[128];
            strncpy(newMask, argv[2], sizeof(newMask));
            newMask[sizeof(newMask) - 1] = '\0';

            char* s = strchr(argv[2], ',');
            while (s) {
                *s = ';';
                s = strchr(s + 1, ',');
            }

            snprintf(command, sizeof(command), "LogMask,%s,", newMask);
            SendCommand(command);
        }
        // Configure the given GPIO to the given mode
        else if ((strncmp(argv[1], "-G", 2) == 0) && argc == 3) {
            snprintf(command, sizeof(command), "SetupExtGPIO,%s,", argv[2]);
            SendCommand(command);
        } else if ((strncmp(argv[1], "-G", 2) == 0) && argc == 4) {
            snprintf(command, sizeof(command), "SetupExtGPIO,%s,%s,", argv[2], argv[3]);
            SendCommand(command);
            // Set the given GPIO to the given value
        } else if ((strncmp(argv[1], "-g", 2) == 0) && argc == 3) {
            snprintf(command, sizeof(command), "ExtGPIO,%s", argv[2]);
            SendCommand(command);
        } else if ((strncmp(argv[1], "-g", 2) == 0) && argc == 5) {
            snprintf(command, sizeof(command), "ExtGPIO,%s,%s,%s", argv[2], argv[3], argv[4]);
            SendCommand(command);
        } else if (strncmp(argv[1], "-r", 2) == 0) {
            // Restart fppd daemon
            snprintf(command, sizeof(command), "restart");
            SendCommand(command);
        } else if ((strncmp(argv[1], "-FBdebug", 8) == 0)) {
            Json::Value val;
            GetFrameBufferDevices(val, true);
            if (val.size() == 0) {
                printf("[]");
            } else {
                std::string js = SaveJsonToString(val);
                printf("%s", js.c_str());
            }
        } else if ((strncmp(argv[1], "-FB", 3) == 0)) {
            Json::Value val;
            GetFrameBufferDevices(val, false);
            if (val.size() == 0) {
                printf("[]");
            } else {
                std::string js = SaveJsonToString(val);
                printf("%s", js.c_str());
            }
        } else {
            Usage(argv[0]);
        }
    } else {
        return 0;
    }

    return 0;
}

/*
 *
 */
void SetupDomainSocket(void) {
    if ((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("fpp client socket");
        return;
    }

    mkdir(FPP_SOCKET_PATH, 0777);
    chmod(FPP_SOCKET_PATH, 0777);

    memset(&client_address, 0, sizeof(struct sockaddr_un));
    client_address.sun_family = AF_UNIX;
    strcpy(client_address.sun_path, FPP_CLIENT_SOCKET);

    unlink(FPP_CLIENT_SOCKET);
    if (bind(socket_fd, (const struct sockaddr*)&client_address,
             sizeof(struct sockaddr_un)) < 0) {
        perror("fpp client bind");
        return;
    }
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, FPP_SERVER_SOCKET);

    symlink(FPP_SERVER_SOCKET, FPP_SERVER_SOCKET_OLD);
    symlink(FPP_CLIENT_SOCKET, FPP_CLIENT_SOCKET_OLD);
}

/*
 *
 */
void SendCommand(const char* com) {
    int max_timeout = 4000;
    int i = 0;
    bytes_sent = sendto(socket_fd, com, strlen(com), 0,
                        (struct sockaddr*)&server_address,
                        sizeof(struct sockaddr_un));

    address_length = sizeof(struct sockaddr_un);
    while (i < max_timeout) {
        i++;
        bytes_received = recvfrom(socket_fd, response, 256, MSG_DONTWAIT,
                                  (struct sockaddr*)&(server_address),
                                  &address_length);
        if ((bytes_received > 0) || ((bytes_received < 0) && (errno != EAGAIN))) {
            break;
        }
        usleep(500);
    }

    close(socket_fd);
    unlink(FPP_CLIENT_SOCKET);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("%s", response);
    } else {
        printf("false");
    }
}

/*
 *
 */
void Usage(char* appname) {
    printf("Usage: %s [OPTION...]\n"
           "\n"
           "fpp is the Falcon Player CLI helper utility.  It can be used to query\n"
           "certain information and send commands to a running fppd daemon.\n"
           "\n"
           "Options:\n"
           "  -q                           - Shutdown fppd daemon\n"
           "  -R                           - Reload schedule config file\n"
           "  -G GPIO MODE                 - Configure the given GPIO to MODE. MODEs include:\n"
           "                                 Input    - Set to Input. For PiFace inputs this only enables the pull-up\n"
           "                                 Output   - Set to Output. (This is not needed for PiFace outputs)\n"
           "                                 SoftPWM  - Set to Software PWM.\n"
           "  -g GPIO MODE VALUE           - Set the given GPIO to VALUE applicable to the given MODEs defined above\n"
           "                                 VALUE is ignored for Input mode\n"
           "  -FB                          - Query usable framebuffer devices\n\n\n"
           " The following commands are now deprecated and the FPP API should be used instead.  Eg http://<fpp hostname>/api/command/Stop Now\n"
           " Further information on the FPP API can be found at http://<fpp hostname>/apihelp.php\n\n"
           "  -V                           - Print version information\n"
           "                                            /api/fppd/version\n"
           "  -r                           - Restart fppd daemon\n"
           "                                            /api/system/fppd/restart\n"
           "  -s                           - Get fppd status\n"
           "                                            /api/system/status\n"
           "  -c PLAYLIST_ACTION           - Perform a playlist action.\n"
           "                                 next     - skip to next item in the playlist\n"
           "                                            /api/command/Next Playlist Item\n"
           "                                 prev     - jump back to previous item\n"
           "                                            /api/command/Prev Playlist Item\n"
           "                                 stop     - stop the playlist immediately\n"
           "                                            /api/command/Stop Now\n"
           "                                 graceful - stop the playlist gracefully\n"
           "                                            /api/command/Stop Gracefully\n"
           "                                 pause    - pause a sequence (not media)\n"
           "                                            /api/command/Pause Playlist\n"
           "                                 step     - single-step a paused sequence\n"
           "                                            No direct replacement\n"
           "                                 stepback - step a paused sequence backwards\n"
           "                                            No direct replacement\n"
           "  -p PLAYLISTNAME [STARTITEM]  - Play Playlist PLAYLISTNAME in repeat mode\n"
           "                                 /api/command/Start Playlist/<PLAYLISTNAME>/<1 or STARTITEM>/true\n"
           "  -P PLAYLISTNAME [STARTITEM]  - Play Playlist PLAYLISTNAME once, optionally\n"
           "                                 starting on item STARTITEM in the playlist\n"
           "                                 /api/command/Start Playlist/<PLAYLISTNAME>/<1 or STARTITEM>/false\n"
           "  -S                           - Stop Playlist gracefully\n"
           "                                 /api/command/Stop Gracefully\n"
           "  -L                           - Stop Playlist gracefully after current loop\n"
           "                                 /api/command/Stop Gracefully/true\n"
           "  -d                           - Stop Playlist immediately\n"
           "                                 /api/command/Stop Now\n"
           "  -e EFFECTNAME[,CH[,LOOP]]    - Start Effect EFFECTNAME with optional\n"
           "                                 start channel set to CH and optional\n"
           "                                 looping if LOOP is set to 1\n"
           "                                 /api/command/Effect Start/EFFECTNAME/[CH]/[True for Loop]\n"
           "  -E EFFECTNAME                - Stop Effect EFFECTNAME\n"
           "                                 /api/command/Effect Stop/EFFECTNAME\n"
           "  -C FPPCOMMAND ARG1 ARG2 ...  - Trigger the FPP Command\n"
           "                                 /api/command/<FPPCOMMAND/ARG1/ARG2/etc\n"
           "  -t CommandPresetSlot         - Trigger FPP Command Preset via Slot # or Preset Name\n"
           "                                 /api/command/Trigger Command Preset Slot/<Slot #>\n"
           "                                 /api/command/Trigger Command Preset/<Preset Name>\n"
           "  -v VOLUME                    - Set volume to 'VOLUME'\n"
           "                                 /api/command/Volume Set/VOLUME\n"
           "\n",
           appname);
}
