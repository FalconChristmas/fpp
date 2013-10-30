#include "fpp.h"
#include "log.h"
#include "command.h"
#include "schedule.h"
#include "playList.h"
#include "ogg123.h"
#include "e131bridge.h"
#include "settings.h"

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
		char NextScheduleStartText[64];
		char NextPlaylist[128];
    switch(command[0])
    {
			case 's':
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
				break; 
			case 'p':
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					StopPlaylistNow();
				}
				sleep(1);
	
				s = strtok(command,",");
				s = strtok(NULL,",");
				if (!s)
				{
					sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
					break;
				}
				strcpy((char*)playlistDetails.currentPlaylistFile,s);
				s = strtok(NULL,",");
				if (s)
					playlistDetails.currentPlaylistEntry = atoi(s);
				playlistDetails.repeat = 1 ;
				playlistDetails.playlistStarting=1;
				FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
				sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				break;
			case 'P':
				if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
				{
					StopPlaylistNow();
				}
				sleep(1);
	
				s = strtok(command,",");
				s = strtok(NULL,",");
				if (!s)
				{
					sprintf(response,"%d,%d,Unknown Playlist,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
					break;
				}
				strcpy((char*)playlistDetails.currentPlaylistFile,s);
				s = strtok(NULL,",");
				if (s)
					playlistDetails.currentPlaylistEntry = atoi(s);
				playlistDetails.repeat = 0;
				playlistDetails.playlistStarting=1;
				FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
				sprintf(response,"%d,%d,Playlist Started,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				break;
			case 'S':
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
				break;
			case 'd':
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
				break;
			case 'R':
				if(FPPstatus==FPP_STATUS_IDLE)
				{
					LoadCurrentScheduleInfo();
				}
				LoadNextScheduleInfo();
				
				
				sprintf(response,"%d,%d,Reloading Schedule,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				break;
	
			case 'v':
				s = strtok(command,",");
				s = strtok(NULL,",");
				if (!s)
				{
					sprintf(response,"%d,%d,Invalid Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_FAILED);
					break;
				}
				setVolume(atoi(s));
				sprintf(response,"%d,%d,Setting Volume,,,,,,,,,,\n",getFPPmode(),COMMAND_SUCCESS);
				break;
	
			case 'w':
				LogWrite("Sending Pixelnet DMX info\n");
				SendPixelnetDMXConfig();
				break;
	
			case 'r':
				WriteBytesReceivedFile();
				sprintf(response,"true\n");
				break;
			default:
				sprintf(response,"Invalid command\n");
		}
  	bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
	LogWrite("%c %s", command[0], response);
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

