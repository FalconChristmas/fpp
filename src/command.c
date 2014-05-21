/*
 *   Command handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "command.h"
#include "schedule.h"
#include "playList.h"
#include "e131bridge.h"
#include "mediaoutput.h"
#include "settings.h"
#include "sequence.h"
#include "effects.h"
#include "playList.h"
#include "FPD.h"
#include "events.h"
#include "channeloutput.h"
#include "E131.h"

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

extern PlaylistDetails playlistDetails;

extern int numberOfSecondsPaused;
extern PlaylistDetails playlistDetails;

 char command[256];
 char response[1056];
 int socket_fd;
 struct sockaddr_un server_address;
 struct sockaddr_un client_address;
 int bytes_received, bytes_sent;
 int integer_buffer;
 socklen_t address_length = sizeof(struct sockaddr_un);

 void Command_Initialize()
 {
   mode_t old_umask;

   LogInfo(VB_COMMAND, "Initializing Command Module\n");
   signal(SIGINT, exit_handler);
   signal(SIGTERM, exit_handler);

   if((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
   {
    perror("server: socket");
   }

   fcntl(socket_fd, F_SETFL, O_NONBLOCK);
   memset(&server_address, 0, sizeof(server_address));
   server_address.sun_family = AF_UNIX;
   strcpy(server_address.sun_path, FPP_SERVER_SOCKET);
   unlink(FPP_SERVER_SOCKET);

   old_umask = umask(0011);

   if(bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0)
   {
    close(socket_fd);
    perror("server: bind");
   }

   umask(old_umask);
 }

 void CloseCommand()
 {
   close(socket_fd);
   unlink(FPP_SERVER_SOCKET);
 }

 void Commandproc()
{
  bzero(command, sizeof(command));
  bytes_received = recvfrom(socket_fd, command,256, 0,
                           (struct sockaddr *) &(client_address),
                            &address_length);

  if(bytes_received > 0)
  {
    ProcessCommand();
  }

 }

  void ProcessCommand()
  {
    char *s;
    char *s2;
    char *response2 = NULL;
    int i;
		char NextScheduleStartText[64] = "";
		char NextPlaylist[128] = "";
		char CommandStr[64];
		s = strtok(command,",");
		strcpy(CommandStr, s);
		if (!strcmp(CommandStr, "s"))
		{
				GetNextScheduleStartText(NextScheduleStartText);
				GetNextPlaylistText(NextPlaylist);
				if(FPPstatus==FPP_STATUS_IDLE)
				{
					sprintf(response,"%d,%d,%d,%s,%s\n",getFPPmode(),0,getVolume(),NextPlaylist,NextScheduleStartText);
				}
				else
				{
					if(playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'b' || playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'm')
					{
						sprintf(response,"%d,%d,%d,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",
										getFPPmode(),FPPstatus,getVolume(),playlistDetails.currentPlaylist,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,
										playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,
										mediaOutputStatus.secondsElapsed,
										mediaOutputStatus.secondsRemaining,
										NextPlaylist,NextScheduleStartText);
					}
					else if (playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 's')
					{
						sprintf(response,"%d,%d,%d,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",getFPPmode(),FPPstatus,getVolume(),
										playlistDetails.currentPlaylist,playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,
										playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,
										seqSecondsElapsed,
										seqSecondsRemaining,
										NextPlaylist,NextScheduleStartText);
					}
					else
					{			
						sprintf(response,"%d,%d,%d,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",getFPPmode(),FPPstatus,getVolume(),playlistDetails.currentPlaylist,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,	
										playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,
										numberOfSecondsPaused,
										(int)playlistDetails.playList[playlistDetails.currentPlaylistEntry].pauselength-numberOfSecondsPaused,
										NextPlaylist,NextScheduleStartText);
					}
				}
		}
		else if (!strcmp(CommandStr, "p"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					StopPlaylistNow();
				}
				sleep(1);
	
				s = strtok(NULL,",");
				if (s)
				{
					strcpy(playlistDetails.currentPlaylistFile,s);
					s = strtok(NULL,",");
					if (s)
						playlistDetails.currentPlaylistEntry = atoi(s);
					playlistDetails.repeat = 1 ;
					playlistDetails.playlistStarting=1;
					FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
					sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "P"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					StopPlaylistNow();
				}
				sleep(1);
	
				s = strtok(NULL,",");
				if (s)
				{
					strcpy(playlistDetails.currentPlaylistFile,s);
					s = strtok(NULL,",");
					if (s)
						playlistDetails.currentPlaylistEntry = atoi(s);
					playlistDetails.repeat = 0;
					playlistDetails.playlistStarting=1;
					FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
					sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "S"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING)
				{
					playlistDetails.ForceStop = 1;
					StopPlaylistGracefully();
					ReLoadCurrentScheduleInfo();
					sprintf(response,"%d,%d,Playlist Stopping Gracefully,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,Not playing,,,,,,,,,,,\n",COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "d"))
		{
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					playlistDetails.ForceStop = 1;
					StopPlaylistNow();
					ReLoadCurrentScheduleInfo();
					sprintf(response,"%d,%d,Playlist Stopping Now,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Not playing,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "R"))
		{
				if(FPPstatus==FPP_STATUS_IDLE)
				{
					LoadCurrentScheduleInfo();
				}
				LoadNextScheduleInfo();
				
				
				sprintf(response,"%d,%d,Reloading Schedule,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
		}
		else if (!strcmp(CommandStr, "v"))
		{
				s = strtok(NULL,",");
				if (s)
				{
					setVolume(atoi(s));
					sprintf(response,"%d,%d,Setting Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				}
				else
				{
					sprintf(response,"%d,%d,Invalid Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
				}
		}
		else if (!strcmp(CommandStr, "w"))
		{
				LogInfo(VB_CHANNELOUT, "Sending Pixelnet DMX info\n");
				SendFPDConfig();
		}
		else if (!strcmp(CommandStr, "r"))
		{
				WriteBytesReceivedFile();
				sprintf(response,"true\n");
		}
		else if (!strcmp(CommandStr, "q"))
		{
			// Quit/Shutdown fppd
			ShutdownFPPD();
		}
		else if (!strcmp(CommandStr, "e"))
		{
			// Start an Effect
			s = strtok(NULL,",");
			s2 = strtok(NULL,",");
			if (s && s2)
			{
				i = StartEffect(s, atoi(s2));
				if (i >= 0)
					sprintf(response,"%d,%d,Starting Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
				else
					sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
			}
			else
				sprintf(response,"%d,%d,Invalid Effect,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "t"))
		{
			// Trigger an event
			s = strtok(NULL,",");
			i = TriggerEventByID(s);
			if (i >= 0)
				sprintf(response,"%d,%d,Event Triggered,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
			else
				sprintf(response,"%d,%d,Event Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "LogLevel"))
		{
			s = strtok(NULL,",");

			if (SetLogLevel(s)) {
				sprintf(response,"%d,%d,Log Level Updated,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
			} else {
				sprintf(response,"%d,%d,Error Updating Log Level,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_FAILED,logLevel,logMask);
			}
		}
		else if (!strcmp(CommandStr, "LogMask"))
		{
			s = strtok(NULL,",");

			if ((s && SetLogMask(s)) || SetLogMask("")) {
				sprintf(response,"%d,%d,Log Mask Updated,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_SUCCESS,logLevel,logMask);
			} else {
				sprintf(response,"%d,%d,Error Updating Log Mask,%d,%d,,,,,,,,,\n",
					getFPPmode(),COMMAND_FAILED,logLevel,logMask);
			}
		}
		else if (!strcmp(CommandStr, "StopEffect"))
		{
			s = strtok(NULL,",");
			i = atoi(s);
			if (StopEffect(i))
					sprintf(response,"%d,%d,Stopping Effect,%d,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS,i);
			else
					sprintf(response,"%d,%d,Stop Effect Failed,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
		}
		else if (!strcmp(CommandStr, "GetRunningEffects"))
		{
			sprintf(response,"%d,%d,Running Effects",getFPPmode(),COMMAND_SUCCESS);
			GetRunningEffects(response, &response2);
		}
		else if (!strcmp(CommandStr, "ReloadChannelRemapData"))
		{
			if ((FPPstatus==FPP_STATUS_IDLE) &&
				(LoadChannelRemapData())) {
				sprintf(response,"%d,%d,Channel Remap Data Reloaded,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
			} else {
				sprintf(response,"%d,%d,Failed to reload Channel Remap Data,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
			}
		}
		else
		{
			sprintf(response,"Invalid command: '%s'\n", CommandStr);
		}

		if (response2)
		{
			bytes_sent = sendto(socket_fd, response2, strlen(response2), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogDebug(VB_COMMAND, "%s %s", CommandStr, response2);
			free(response2);
			response2 = NULL;
		}
		else
		{
			bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogDebug(VB_COMMAND, "%s %s", CommandStr, response);
		}
  }

  void exit_handler(int signum)
	{
     LogInfo(VB_GENERAL, "Caught signal %d\n",signum);
     CloseCommand();
		 if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
		 {
		 		CloseMediaOutput();
		 }
	   exit(signum);
	}

