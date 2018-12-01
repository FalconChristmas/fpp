/*
 *   Command handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "falcon.h"
#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "command.h"
#include "Scheduler.h"
#include "e131bridge.h"
#include "mediaoutput.h"
#include "settings.h"
#include "Sequence.h"
#include "effects.h"
#include "playlist/Playlist.h"
#include "Plugins.h"
#include "FPD.h"
#include "events.h"
#include "channeloutput.h"
#include "E131.h"
#include "gpio.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <jsoncpp/json/json.h>

extern PluginCallbackManager pluginCallbackManager;

 int socket_fd;
 struct sockaddr_un server_address;
 int integer_buffer;
 int fppdStartTime = 0;


static void exit_handler(int signum)
{
    LogInfo(VB_GENERAL, "Caught signal %d\n",signum);
    CloseCommand();
    if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING) {
        CloseMediaOutput();
    }
    exit(signum);
}

 int Command_Initialize()
 {
     mode_t old_umask;

     LogInfo(VB_COMMAND, "Initializing Command Module\n");
     signal(SIGINT, exit_handler);
     signal(SIGTERM, exit_handler);

     if((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
         perror("server: socket");
         exit(1);
     }

     fcntl(socket_fd, F_SETFL, O_NONBLOCK);
     memset(&server_address, 0, sizeof(server_address));
     server_address.sun_family = AF_UNIX;
     strcpy(server_address.sun_path, FPP_SERVER_SOCKET);
     unlink(FPP_SERVER_SOCKET);

     old_umask = umask(0011);

     if(bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
         close(socket_fd);
         perror("command socket bind");
         exit(1);
     }

     umask(old_umask);

     fppdStartTime = time(NULL);

     return socket_fd;
 }

 void CloseCommand()
 {
     close(socket_fd);
     unlink(FPP_SERVER_SOCKET);
 }



char *ProcessCommand(char *command, char *response)
{
    char *s;
    char *s2;
    char *s3;
    char *s4;
    char *response2 = NULL;
    int i;
    char NextPlaylist[128] = "No playlist scheduled.";
    char NextScheduleStartText[64] = "";
    char CommandStr[64];
    LogExcess(VB_COMMAND, "CMD: %s\n", command);
    s = strtok(command,",");
    strcpy(CommandStr, s);
    if (!strcmp(CommandStr, "s")) {
        scheduler->GetNextScheduleStartText(NextScheduleStartText);
        scheduler->GetNextPlaylistText(NextPlaylist);
        if(FPPstatus==FPP_STATUS_IDLE) {
            if (getFPPmode() == REMOTE_MODE) {
                int secsElapsed = 0;
                int secsRemaining = 0;
                char seqFilename[1024];
                char mediaFilename[1024];

                if (sequence->IsSequenceRunning()) {
                    strcpy(seqFilename, sequence->m_seqFilename);
                    secsElapsed = sequence->m_seqSecondsElapsed;
                    secsRemaining = sequence->m_seqSecondsRemaining;
                } else {
                    strcpy(seqFilename, "");
                }

                if (mediaOutput) {
                    strcpy(mediaFilename, mediaOutput->m_mediaFilename.c_str());
                    secsElapsed = mediaOutputStatus.secondsElapsed;
                    secsRemaining = mediaOutputStatus.secondsRemaining;
                } else {
                    strcpy(mediaFilename, "");
                }

                sprintf(response,"%d,%d,%d,%s,%s,%d,%d\n",
                        getFPPmode(), 0, getVolume(), seqFilename,
                        mediaFilename, secsElapsed, secsRemaining);
            } else if (sequence->IsSequenceRunning()) {
                sprintf(response,"%d,%d,%d,,,%s,,0,0,%d,%d,%s,%s,0\n",
                        getFPPmode(),
                        1,
                        getVolume(),
                        sequence->m_seqFilename,
                        sequence->m_seqSecondsElapsed,
                        sequence->m_seqSecondsRemaining,
                        NextPlaylist,
                        NextScheduleStartText);
            } else {
                sprintf(response,"%d,%d,%d,%s,%s\n",getFPPmode(),0,getVolume(),NextPlaylist,NextScheduleStartText);
            }
        } else {
            Json::Value pl = playlist->GetInfo();
            if ((pl["currentEntry"]["type"] == "both") ||
                (pl["currentEntry"]["type"] == "media")) {
                //printf(" %s\n", pl.toStyledString().c_str());
                sprintf(response,"%d,%d,%d,%s,%s,%s,%s,%d,%d,%d,%d,%s,%s,%d\n",
                    getFPPmode(),
                    FPPstatus,
                    getVolume(),
                    pl["name"].asString().c_str(),
                    pl["currentEntry"]["type"].asString().c_str(),
                    pl["currentEntry"]["type"].asString() == "both" ? pl["currentEntry"]["sequence"]["sequenceName"].asString().c_str() : "",
                    pl["currentEntry"]["type"].asString() == "both"
                        ? pl["currentEntry"]["media"]["mediaFilename"].asString().c_str()
                        : pl["currentEntry"]["mediaFilename"].asString().c_str() ,
//							pl["currentEntry"]["entryID"].asInt() + 1,
                    playlist->GetPosition(),
                    pl["size"].asInt(),
                    pl["currentEntry"]["type"].asString() == "both"
                        ? pl["currentEntry"]["media"]["secondsElapsed"].asInt()
                        : pl["currentEntry"]["secondsElapsed"].asInt(),
                    pl["currentEntry"]["type"].asString() == "both"
                        ? pl["currentEntry"]["media"]["secondsRemaining"].asInt()
                        : pl["currentEntry"]["secondsRemaining"].asInt(),
                    NextPlaylist,
                    NextScheduleStartText,
                    pl["repeat"].asInt());
            } else if (pl["currentEntry"]["type"] == "sequence") {
                sprintf(response,"%d,%d,%d,%s,%s,%s,%s,%d,%d,%d,%d,%s,%s,%d\n",
                    getFPPmode(),
                    FPPstatus,
                    getVolume(),
                    pl["name"].asString().c_str(),
                    pl["currentEntry"]["type"].asString().c_str(),
                    pl["currentEntry"]["sequence"]["sequenceName"].asString().c_str(),
                    "",
//							pl["currentEntry"]["entryID"].asInt() + 1,
                    playlist->GetPosition(),
                    pl["size"].asInt(),
                    sequence->m_seqSecondsElapsed,
                    sequence->m_seqSecondsRemaining,
                    NextPlaylist,
                    NextScheduleStartText,
                    pl["repeat"].asInt());
            } else {
                sprintf(response,"%d,%d,%d,%s,%s,%s,%s,%d,%d,%d,%d,%s,%s,%d\n",
                    getFPPmode(),
                    FPPstatus,
                    getVolume(),
                    pl["name"].asString().c_str(),
                    pl["currentEntry"]["type"].asString().c_str(),
                    "",
                    "",
//							pl["currentEntry"]["entryID"].asInt() + 1,
                    playlist->GetPosition(),
                    pl["size"].asInt(),
                    pl["currentEntry"]["type"].asString() == "pause" ? pl["currentEntry"]["duration"].asInt() - pl["currentEntry"]["remaining"].asInt() : 0,
                    pl["currentEntry"]["type"].asString() == "pause" ? pl["currentEntry"]["remaining"].asInt() : 0,
                    NextPlaylist,
                    NextScheduleStartText,
                    pl["repeat"].asInt());
            }
        }
    } else if ((!strcmp(CommandStr, "P")) || (!strcmp(CommandStr, "p"))) {
        s = strtok(NULL,",");
        s2 = strtok(NULL,",");

        int entry = 0;
        if (s2 && s2[0])
            entry = atoi(s2);

        if (s && playlist->Play(s, entry, strcmp(CommandStr, "p") ? 0 : 1)) {
            FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
            sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
        } else {
            sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "S")) {
        if (FPPstatus==FPP_STATUS_PLAYLIST_PLAYING) {
            playlist->StopGracefully(1);
            scheduler->ReLoadCurrentScheduleInfo();
            sprintf(response,"%d,%d,Playlist Stopping Gracefully,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
        } else {
            sprintf(response,"%d,Not playing,,,,,,,,,,,\n",COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "d")) {
        if (FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY) {
            playlist->StopNow(1);
            scheduler->ReLoadCurrentScheduleInfo();
            sprintf(response,"%d,%d,Playlist Stopping Now,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
        } else if ((FPPstatus == FPP_STATUS_IDLE) &&
                   (sequence->IsSequenceRunning())) {
            sequence->CloseSequenceFile();
            sprintf(response,"%d,%d,Sequence Stopping Now,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
        } else {
            sprintf(response,"%d,%d,Not playing,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "R")) {
        if (FPPstatus==FPP_STATUS_IDLE) {
            scheduler->ReLoadCurrentScheduleInfo();
        }
        scheduler->ReLoadNextScheduleInfo();
        sprintf(response,"%d,%d,Reloading Schedule,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
    } else if (!strcmp(CommandStr, "v")) {
        s = strtok(NULL,",");
        if (s) {
            setVolume(atoi(s));
            sprintf(response,"%d,%d,Setting Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
        } else {
            sprintf(response,"%d,%d,Invalid Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "w")) {
        LogInfo(VB_SETTING, "Sending Falcon hardware config\n");
        if (!DetectFalconHardware(1))
            SendFPDConfig();
    } else if (!strcmp(CommandStr, "q")) {
        // Quit/Shutdown fppd
        if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
            (FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)) {
            playlist->StopNow(1);
            sleep(2);
        }

        ShutdownFPPD();

        sleep(1);
    } else if (!strcmp(CommandStr, "e")) {
        // Start an Effect
        s = strtok(NULL,",");
        s2 = strtok(NULL,",");
        s3 = strtok(NULL,",");
        if (s && s2) {
            i = StartEffect(s, atoi(s2), atoi(s3));
            if (i >= 0)
                sprintf(response,"%d,%d,Starting Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
            else
                sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
        } else
            sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
    } else if (!strcmp(CommandStr, "t")) {
        // Trigger an event
        s = strtok(NULL,",");
        pluginCallbackManager.eventCallback(s, "command");
        i = TriggerEventByID(s);
        if (i >= 0)
            sprintf(response,"%d,%d,Event Triggered,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
        else
            sprintf(response,"%d,%d,Event Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
    } else if (!strcmp(CommandStr, "GetTestMode")) {
        strcpy(response, channelTester->GetConfig().c_str());
        strcat(response, "\n");
    } else if (!strcmp(CommandStr, "SetTestMode")) {
        if (channelTester->SetupTest(std::string(s + strlen(s) + 1)))
        {
            sprintf(response, "0,%d,Test Mode Activated,,,,,,,,,\n",
                COMMAND_SUCCESS);
        } else {
            sprintf(response, "0,%d,Test Mode Deactivated,,,,,,,,,\n",
                COMMAND_SUCCESS);
        }
    } else if (!strcmp(CommandStr, "LogLevel")) {
        s = strtok(NULL,",");

        if (SetLogLevel(s)) {
            sprintf(response,"%d,%d,Log Level Updated,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
        } else {
            sprintf(response,"%d,%d,Error Updating Log Level,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_FAILED,logLevel,logMask);
        }
    } else if (!strcmp(CommandStr, "LogMask")) {
        s = strtok(NULL,",");

        if ((s && SetLogMask(s)) || SetLogMask("")) {
            sprintf(response,"%d,%d,Log Mask Updated,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
        } else {
            sprintf(response,"%d,%d,Error Updating Log Mask,%d,%d,,,,,,,,,\n",
                getFPPmode(),COMMAND_FAILED,logLevel,logMask);
        }
    } else if (!strcmp(CommandStr, "SetSetting")) {
        char name[128];

        s = strtok(NULL,",");
        if (s) {
            strcpy(name, s);
            s = strtok(NULL,",");
            if (s)
                parseSetting(name, s);
        }
    } else if (!strcmp(CommandStr, "StopAllEffects")) {
        StopAllEffects();
        sprintf(response,"%d,%d,All Effects Stopped,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
    } else if (!strcmp(CommandStr, "StopEffectByName")) {
        s = strtok(NULL,",");
        if (strlen(s)) {
            if (StopEffect(s))
                sprintf(response,"%d,%d,Stopping Effect,%s,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,s);
            else
                sprintf(response,"%d,%d,Stop Effect Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
        }
    } else if (!strcmp(CommandStr, "StopEffect")) {
        s = strtok(NULL,",");
        i = atoi(s);
        if (StopEffect(i))
            sprintf(response,"%d,%d,Stopping Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
        else
            sprintf(response,"%d,%d,Stop Effect Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
    } else if (!strcmp(CommandStr, "GetRunningEffects")) {
        sprintf(response,"%d,%d,Running Effects",getFPPmode(),COMMAND_SUCCESS);
        GetRunningEffects(response, &response2);
    } else if (!strcmp(CommandStr, "GetFPPDUptime")) {
        sprintf(response,"%d,%d,FPPD Uptime,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS, time(NULL) - fppdStartTime);
    } else if (!strcmp(CommandStr, "StartSequence")) {
        if ((FPPstatus == FPP_STATUS_IDLE) &&
            (!sequence->IsSequenceRunning())) {
            s = strtok(NULL,",");
            s2 = strtok(NULL,",");
            if (s && s2) {
                i = atoi(s2);
                sequence->OpenSequenceFile(s, 0, i);
            } else {
                LogDebug(VB_COMMAND, "Invalid command: %s\n", command);
            }
        } else {
            LogErr(VB_COMMAND, "Tried to start a sequence when a playlist or "
                    "sequence is already running\n");
        }
    } else if (!strcmp(CommandStr, "StopSequence")) {
        if ((FPPstatus == FPP_STATUS_IDLE) &&
            (sequence->IsSequenceRunning())) {
            sequence->CloseSequenceFile();
        } else {
            LogDebug(VB_COMMAND,
                "Tried to stop a sequence when no sequence is running\n");
        }
    } else if (!strcmp(CommandStr, "ToggleSequencePause")) {
        if ((sequence->IsSequenceRunning()) &&
            ((FPPstatus == FPP_STATUS_IDLE) ||
             ((FPPstatus != FPP_STATUS_IDLE) &&
              (playlist->GetInfo()["currentEntry"]["type"] == "sequence")))) {
            sequence->ToggleSequencePause();
        }
    } else if (!strcmp(CommandStr, "SingleStepSequence")) {
        if ((sequence->IsSequenceRunning()) &&
            (sequence->SequenceIsPaused()) &&
            ((FPPstatus == FPP_STATUS_IDLE) ||
             ((FPPstatus != FPP_STATUS_IDLE) &&
              (playlist->GetInfo()["currentEntry"]["type"] == "sequence")))) {
            sequence->SingleStepSequence();
        }
    } else if (!strcmp(CommandStr, "SingleStepSequenceBack")) {
        if ((sequence->IsSequenceRunning()) &&
            (sequence->SequenceIsPaused()) &&
            ((FPPstatus == FPP_STATUS_IDLE) ||
             ((FPPstatus != FPP_STATUS_IDLE) &&
              (playlist->GetInfo()["currentEntry"]["type"] == "sequence")))) {
            sequence->SingleStepSequenceBack();
        }
    } else if (!strcmp(CommandStr, "NextPlaylistItem")) {
        switch (FPPstatus)
        {
            case FPP_STATUS_IDLE:
                sprintf(response,"%d,%d,No playlist running\n",getFPPmode(),COMMAND_FAILED);
                break;
            case FPP_STATUS_PLAYLIST_PLAYING:
                sprintf(response,"%d,%d,Skipping to next playlist item\n",getFPPmode(),COMMAND_SUCCESS);
                playlist->NextItem();
                break;
            case FPP_STATUS_STOPPING_GRACEFULLY:
                sprintf(response,"%d,%d,Playlist is stopping gracefully\n",getFPPmode(),COMMAND_FAILED);
                break;
        }
    } else if (!strcmp(CommandStr, "PrevPlaylistItem")) {
        switch (FPPstatus) {
            case FPP_STATUS_IDLE:
                sprintf(response,"%d,%d,No playlist running\n",getFPPmode(),COMMAND_FAILED);
                break;
            case FPP_STATUS_PLAYLIST_PLAYING:
                sprintf(response,"%d,%d,Skipping to previous playlist item\n",getFPPmode(),COMMAND_SUCCESS);
                playlist->PrevItem();
                break;
            case FPP_STATUS_STOPPING_GRACEFULLY:
                sprintf(response,"%d,%d,Playlist is stopping gracefully\n",getFPPmode(),COMMAND_FAILED);
                break;
        }
    } else if (!strcmp(CommandStr, "SetupExtGPIO")) {
        // Configure the given GPIO to the given mode
        s = strtok(NULL,",");
        s2 = strtok(NULL,",");
        if (s && s2)
        {
            
            if (!SetupExtGPIO(atoi(s), s2))
            {
                sprintf(response, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,atoi(s),s2);
            } else {
                sprintf(response, "%d,%d,Configuring GPIO,%d,%s,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED,atoi(s),s2);
            }
        }
    } else if (!strcmp(CommandStr, "ExtGPIO")) {
        s = strtok(NULL,",");
        s2 = strtok(NULL,",");
        s3 = strtok(NULL,",");
        if (s && s2 && s3)
        {
            i = ExtGPIO(atoi(s), s2, atoi(s3));
            if (i >= 0)
            {
                sprintf(response, "%d,%d,Setting GPIO,%d,%s,%d,%d,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,atoi(s),s2,atoi(s3),i);
            } else {
                sprintf(response, "%d,%d,Setting GPIO,%d,%s,%d,,,,,,,,\n",getFPPmode(),COMMAND_FAILED,atoi(s),s2,atoi(s3));
            }
        }
    } else {
        sprintf(response,"Invalid command: '%s'\n", CommandStr);
    }

    return response2;
}

void CommandProc()
{
    char command[256];
    char ocommand[256];

    struct sockaddr_un client_address;
    int bytes_received, bytes_sent;
    socklen_t address_length = sizeof(struct sockaddr_un);

    bzero(command, sizeof(command));
    bytes_received = recvfrom(socket_fd, command, 256, 0,
                              (struct sockaddr *) &(client_address),
                              &address_length);
    
    if (bytes_received <= 0) {
        LogWarn(VB_COMMAND, "Command socket received but no bytes\n");
    }

    while (bytes_received > 0) {
        memcpy(ocommand, command, bytes_received);
        ocommand[bytes_received] = 0;
        char response[1500] = "\n";
        char *response2 = ProcessCommand(command, response);
        errno = 0;
        if (response2) {
            bytes_sent = sendto(socket_fd, response2, strlen(response2), 0,
                                (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
            LogDebug(VB_COMMAND, "%s - (%d %d %s) %s", ocommand, bytes_sent, errno, strerror(errno), response2);
            free(response2);
            response2 = NULL;
        } else {
            bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                                (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
            LogDebug(VB_COMMAND, "%s - (%d %d %s) %s", ocommand, bytes_sent, errno, strerror(errno), response);
        }
        
        bzero(command, sizeof(command));
        bytes_received = recvfrom(socket_fd, command, 256, 0,
                                  (struct sockaddr *) &(client_address),
                                  &address_length);
    }
}
