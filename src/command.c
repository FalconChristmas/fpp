#include "fpp.h"
#include "log.h"
#include "command.h"
#include "schedule.h"
#include "playList.h"
#include "ogg123.h"
#include "e131bridge.h"
#include "settings.h"
#include "effects.h"

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

extern int FPPstatus;

extern PlaylistDetails playlistDetails;

extern struct mpg123_type mpg123;
extern int MusicPlayerStatus;
extern MusicStatus musicStatus;
extern int E131secondsElasped;
extern int E131secondsRemaining;
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

   LogWrite("Initializing Command Module\n");
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
		char NextScheduleStartText[64];
		char NextPlaylist[128];
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
										musicStatus.secondsElasped,
										musicStatus.secondsRemaining,
										NextPlaylist,NextScheduleStartText);
					}
					else if (playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 's')
					{
						sprintf(response,"%d,%d,%d,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",getFPPmode(),FPPstatus,getVolume(),
										playlistDetails.currentPlaylist,playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
										playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,
										playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,
										E131secondsElasped,
										E131secondsRemaining,
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
				if (!s)
				{
					strcpy((char*)playlistDetails.currentPlaylistFile,s);
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
					strcpy((char*)playlistDetails.currentPlaylistFile,s);
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
				LogWrite("Sending Pixelnet DMX info\n");
				SendPixelnetDMXConfig();
		}
		else if (!strcmp(CommandStr, "W"))
		{
				WriteBytesReceivedFile();
				sprintf(response,"true\n");
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
		else
		{
			sprintf(response,"Invalid command: '%s'\n", CommandStr);
		}

		if (response2)
		{
			bytes_sent = sendto(socket_fd, response2, strlen(response2), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogWrite("%s %s", CommandStr, response2);
			free(response2);
			response2 = NULL;
		}
		else
		{
			bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
			LogWrite("%s %s", CommandStr, response);
		}
  }

  void exit_handler(int signum)
	{
     LogWrite("Caught signal %d\n",signum);
     CloseCommand();
		 if(MusicPlayerStatus == PLAYING_MPLAYER_STATUS)
		 {
		 		OGGstopSong();
		 }
	   exit(signum);
	}

