/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "command.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <array>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include "Player.h"
#include "Scheduler.h"
#include "Sequence.h"
#include "Warnings.h"
#include "common.h"
#include "effects.h"
#include "fpp.h"
#include "gpio.h"
#include "log.h"
#include "settings.h"
#include "channeltester/ChannelTester.h"
#include "commands/Commands.h"
#include "mediaoutput/MediaOutputBase.h"
#include "mediaoutput/MediaOutputStatus.h"
#include "mediaoutput/mediaoutput.h"
#include "playlist/Playlist.h"

constexpr int MAX_RESPONSE_SIZE = 1501;

int socket_fd = -1;
struct sockaddr_un server_address;
int integer_buffer;
int fppdStartTime = 0;
int m_localOverride;

static void exit_handler(int signum) {
    LogInfo(VB_GENERAL, "Caught signal %d\n", signum);
    char buf[256] = { 0 };
    if (mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING) {
        CloseMediaOutput();
    }
    ShutdownFPPD();
    sleep(1);

    CloseCommand();
}

int Command_Initialize() {
    mode_t old_umask;

    LogInfo(VB_COMMAND, "Initializing Command Module\n");
    signal(SIGINT, exit_handler);
    signal(SIGTERM, exit_handler);

    if ((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("server: socket");
        exit(1);
    }

    mkdir(FPP_SOCKET_PATH, 0777);
    chmod(FPP_SOCKET_PATH, 0777);

    fcntl(socket_fd, F_SETFL, O_NONBLOCK);
    memset(&server_address, 0, sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, FPP_SERVER_SOCKET);
    unlink(FPP_SERVER_SOCKET);

    old_umask = umask(0011);

    if (bind(socket_fd, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        close(socket_fd);
        perror("command socket bind");
        exit(1);
    }

    symlink(FPP_SERVER_SOCKET, FPP_SERVER_SOCKET_OLD);
    umask(old_umask);

    fppdStartTime = time(NULL);

    return socket_fd;
}

void CloseCommand() {
    if (socket_fd >= 0) {
        close(socket_fd);
        unlink(FPP_SERVER_SOCKET);
        socket_fd = -1;
    }
}

char* ProcessCommand(char* command, char* response) {
    char* s;
    char* s2;
    char* s3;
    char* s4;
    char* response2 = NULL;
    int i;
    std::string NextPlaylist = "No playlist scheduled.";
    std::string NextPlaylistStart = "";
    char CommandStr[64];
    LogExcess(VB_COMMAND, "CMD: %s\n", command);
    s = strtok(command, ",");
    strncpy(CommandStr, s, sizeof(CommandStr)); // s can be 256 bytes long
    CommandStr[sizeof(CommandStr) - 1] = '\0';

    if ((!strcmp(CommandStr, "d")) ||
        (!strcmp(CommandStr, "StopNow"))) {
        if ((Player::INSTANCE.GetStatus() == FPP_STATUS_PLAYLIST_PLAYING) ||
            (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY) ||
            (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
            Player::INSTANCE.StopNow(1);
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Playlist Stopping Now,,,,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS);
        } else if ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) &&
                   (sequence->IsSequenceRunning())) {
            sequence->CloseSequenceFile();
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Sequence Stopping Now,,,,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Not playing,,,,,,,,,,\n", getFPPmode(), COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "R")) {
        scheduler->ReloadScheduleFile();
        snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Reloading Schedule,,,,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS);
    } else if (!strcmp(CommandStr, "v")) {
        s = strtok(NULL, ",");
        if (s) {
            setVolume(atoi(s));
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Setting Volume,,,,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Invalid Volume,,,,,,,,,,\n", getFPPmode(), COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "q")) {
        // Quit/Shutdown fppd
        if ((Player::INSTANCE.GetStatus() == FPP_STATUS_PLAYLIST_PLAYING) ||
            (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY) ||
            (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
            Player::INSTANCE.StopNow(1);
            sleep(2);
        }

        ShutdownFPPD();

        sleep(1);
    } else if (!strcmp(CommandStr, "restart")) {
        ShutdownFPPD(true);
    } else if (!strcmp(CommandStr, "GetTestMode")) {
        strcpy(response, ChannelTester::INSTANCE.GetConfig().c_str());
        strcat(response, "\n");
    } else if (!strcmp(CommandStr, "SetTestMode")) {
        if (ChannelTester::INSTANCE.SetupTest(std::string(s + strlen(s) + 1))) {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "0,%d,Test Mode Activated,,,,,,,,,\n",
                     COMMAND_SUCCESS);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "0,%d,Test Mode Deactivated,,,,,,,,,\n",
                     COMMAND_SUCCESS);
        }
    } else if (!strcmp(CommandStr, "LogLevel")) {
        s = strtok(NULL, ",");
        s2 = NULL;
        if (s != NULL)
            s2 = strtok(NULL, ",");

        if (FPPLogger::INSTANCE.SetLevel(s, s2)) {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Log Level Updated,%s,%s,,,,,,,,,\n",
                     getFPPmode(), COMMAND_SUCCESS, s, s2);

            WarningHolder::RemoveWarning(2, EXCESSIVE_LOG_LEVEL_WARNING);
            WarningHolder::RemoveWarning(3, DEBUG_LOG_LEVEL_WARNING);
            int lowestLogLevel = FPPLogger::INSTANCE.MinimumLogLevel();
            if (lowestLogLevel == LOG_EXCESSIVE)
                WarningHolder::AddWarning(2, EXCESSIVE_LOG_LEVEL_WARNING);
            else if (lowestLogLevel == LOG_DEBUG)
                WarningHolder::AddWarning(3, DEBUG_LOG_LEVEL_WARNING);
        } else {
            snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Error Updating Log Level,%s,%s,,,,,,,,,\n",
                     getFPPmode(), COMMAND_FAILED, s, s2);
        }
        /*
    } else if (!strcmp(CommandStr, "LogMask")) {
        s = strtok(NULL,",");

        if ((s && SetLogMask(s)) || SetLogMask("")) {
            snprintf(response,MAX_RESPONSE_SIZE-1,"%d,%d,Log Mask Updated,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
        } else {
            snprintf(response,MAX_RESPONSE_SIZE-1,"%d,%d,Error Updating Log Mask,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_FAILED,logLevel,logMask);
        }
*/
    } else if (!strcmp(CommandStr, "SetSetting")) {
        char name[128];

        s = strtok(NULL, ",");
        if (s) {
            strcpy(name, s);
            s = strtok(NULL, ",");
            if (s)
                SetSetting(name, s);
        }
    } else if (!strcmp(CommandStr, "StartSequence")) {
        if ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) &&
            (!sequence->IsSequenceRunning())) {
            s = strtok(NULL, ",");
            s2 = strtok(NULL, ",");
            if (s && s2) {
                i = atoi(s2);
                sequence->OpenSequenceFile(s, 0, i);
                sequence->StartSequence();
            } else {
                LogDebug(VB_COMMAND, "Invalid command: %s\n", command);
            }
        } else {
            LogErr(VB_COMMAND, "Tried to start a sequence when a playlist or "
                               "sequence is already running\n");
        }
    } else if (!strcmp(CommandStr, "StopSequence")) {
        if ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) &&
            (sequence->IsSequenceRunning())) {
            sequence->CloseSequenceFile();
        } else {
            LogDebug(VB_COMMAND,
                     "Tried to stop a sequence when no sequence is running\n");
        }
    } else if (!strcmp(CommandStr, "ToggleSequencePause")) {
        if ((sequence->IsSequenceRunning()) &&
            ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
             ((Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) &&
              (Player::INSTANCE.GetInfo()["currentEntry"]["type"] == "sequence")))) {
            sequence->ToggleSequencePause();
        }
    } else if (!strcmp(CommandStr, "SingleStepSequence")) {
        if ((sequence->IsSequenceRunning()) &&
            (sequence->SequenceIsPaused()) &&
            ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
             ((Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) &&
              (Player::INSTANCE.GetInfo()["currentEntry"]["type"] == "sequence")))) {
            sequence->SingleStepSequence();
        }
// Has been broken for some time issue #986
//    } else if (!strcmp(CommandStr, "SingleStepSequenceBack")) {
//        if ((sequence->IsSequenceRunning()) &&
//            (sequence->SequenceIsPaused()) &&
//            ((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
//             ((Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) &&
//              (Player::INSTANCE.GetInfo()["currentEntry"]["type"] == "sequence")))) {
//            sequence->SingleStepSequenceBack();
//        }
    } else if (!strcmp(CommandStr, "SetupExtGPIO")) {
        // Configure the given GPIO to the given mode
        s = strtok(NULL, ",");
        s2 = strtok(NULL, ",");
        if (s && s2) {
            if (!SetupExtGPIO(atoi(s), s2)) {
                snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS, atoi(s), s2);
            } else {
                snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n", getFPPmode(), COMMAND_FAILED, atoi(s), s2);
            }
        }
    } else if (!strcmp(CommandStr, "ExtGPIO")) {
        s = strtok(NULL, ",");
        s2 = strtok(NULL, ",");
        s3 = strtok(NULL, ",");
        if (s && s2 && s3) {
            i = ExtGPIO(atoi(s), s2, atoi(s3));
            if (i >= 0) {
                snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Setting GPIO,%d,%s,%d,%d,,,,,,,\n", getFPPmode(), COMMAND_SUCCESS, atoi(s), s2, atoi(s3), i);
            } else {
                snprintf(response, MAX_RESPONSE_SIZE - 1, "%d,%d,Setting GPIO,%d,%s,%d,,,,,,,,\n", getFPPmode(), COMMAND_FAILED, atoi(s), s2, atoi(s3));
            }
        }
    } else {
        snprintf(response, MAX_RESPONSE_SIZE - 1, "Invalid command: '%s'\n", CommandStr);
    }

    return response2;
}

void CommandProc() {
    constexpr int MAX_COMMAND_SIZE = 4096;
    std::array<char, MAX_COMMAND_SIZE> command;

    struct sockaddr_un client_address;
    int bytes_received, bytes_sent;
    socklen_t address_length = sizeof(struct sockaddr_un);

    memset(&command[0], 0, MAX_COMMAND_SIZE);
    bytes_received = recvfrom(socket_fd, &command[0], MAX_COMMAND_SIZE - 1, 0,
                              (struct sockaddr*)&(client_address),
                              &address_length);

    if (bytes_received <= 0) {
        LogWarn(VB_COMMAND, "Command socket received but no bytes\n");
    }

    while (bytes_received > 0) {
        command[bytes_received] = 0;

        std::array<char, MAX_RESPONSE_SIZE> response;
        response[0] = 0;
        response[MAX_RESPONSE_SIZE - 1] = '\n';
        char* response2 = ProcessCommand(&command[0], &response[0]);
        errno = 0;
        if (response2) {
            bytes_sent = sendto(socket_fd, response2, strlen(response2), 0,
                                (struct sockaddr*)&(client_address), sizeof(struct sockaddr_un));
            LogDebug(VB_COMMAND, "%s - (%d %d %s) %s", &command[0], bytes_sent, errno, strerror(errno), response2);
            free(response2);
            response2 = NULL;
        } else {
            bytes_sent = sendto(socket_fd, &response[0], strlen(&response[0]), 0,
                                (struct sockaddr*)&(client_address), sizeof(struct sockaddr_un));
            LogDebug(VB_COMMAND, "%s - (%d %d %s) %s",
                     &command[0], bytes_sent, errno, strerror(errno), &response[0]);
        }

        memset(&command[0], 0, MAX_COMMAND_SIZE);
        bytes_received = recvfrom(socket_fd, &command[0], MAX_COMMAND_SIZE - 1, 0,
                                  (struct sockaddr*)&(client_address),
                                  &address_length);
    }
}
