#include "fpp.h"
#include "log.h"
#include "command.h"
#include "schedule.h"
#include "playList.h"
#include "mpg123.h"
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
extern int FPPmode;


extern PlaylistDetails playlistDetails;

extern struct mpg123_type mpg123;
extern char MPG123volume[4];

extern int E131secondsElasped;
extern int E131secondsRemaining;
extern int numberOfSecondsPaused;

 char command[256];
 char response[256];
 int socket_fd;
 struct sockaddr_un server_address;
 struct sockaddr_un client_address;
 int bytes_received, bytes_sent;
 int integer_buffer;
 socklen_t address_length = sizeof(struct sockaddr_un);

 void Command_Initialize()
 {
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
   strcpy(server_address.sun_path, "/tmp/FPPD");
   unlink("/tmp/FPPD");

   if(bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0)
   {
    close(socket_fd);
    perror("server: bind");
   }
 }

 void CloseCommand()
 {
   unlink("/tmp/FPPD");
   close(socket_fd);
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
		int volume;
		char NextScheduleStartText[64];
		char NextPlaylist[128];
		GetNextScheduleStartText(NextScheduleStartText);
		GetNextPlaylistText(NextPlaylist);
    switch(command[0])
    {
    case 's':
      if(FPPstatus==FPP_STATUS_IDLE)
      {
        sprintf(response,"%d,%d,%s,%s\n",FPPmode,FPPstatus,NextPlaylist,NextScheduleStartText);
      }
      else
      {
				if(playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'b' || playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 'm')
				{
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",
			  					FPPmode,FPPstatus,MPG123volume,playlistDetails.currentPlaylist,
									playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
									playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,
									playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,
									playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,
									(int)mpg123.seconds,(int)mpg123.secondsleft,NextPlaylist,NextScheduleStartText);
				}
				else if (playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType == 's')
				{
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",FPPmode,FPPstatus,MPG123volume,
        					playlistDetails.currentPlaylist,playlistDetails.playList[playlistDetails.currentPlaylistEntry].cType,
									playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName,playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName,
									playlistDetails.currentPlaylistEntry+1,playlistDetails.playListCount,E131secondsElasped,E131secondsRemaining,
				  				NextPlaylist,NextScheduleStartText);
				}
				else
				{			
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",FPPmode,FPPstatus,MPG123volume,playlistDetails.currentPlaylist,
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
			LogWrite("parse1=%s\n",s);
      s = strtok(NULL,",");
			LogWrite("parse2=%s\n",s);
      strcpy(playlistDetails.currentPlaylistFile,s);
      s = strtok(NULL,",");
			LogWrite("parse3=%s\n",s);
		  playlistDetails.currentPlaylistEntry = atoi(s);
			playlistDetails.playlistStarting=1;
			FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
			
      sprintf(response,"%d,Playlist Started,,,,,,,,,,\n",COMMAND_SUCCESS);
      break;
    case 'P':
      if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
      {
        StopPlaylistNow();
      }
      sleep(1);

      s = strtok(command,",");
  		LogWrite("parse1=%s\n",s);
      s = strtok(NULL,",");
  		LogWrite("parse2=%s\n",s);
      strcpy(playlistDetails.currentPlaylistFile,s);
      s = strtok(NULL,",");
			LogWrite("parse3=%s\n",s);
		  playlistDetails.currentPlaylistEntry = atoi(s);
			playlistDetails.playlistStarting=1;
      FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
      sprintf(response,"%d,Playlist Started,,,,,,,,,,\n",COMMAND_SUCCESS);
      break;
    case 'S':
      if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING)
      {
        StopPlaylistGracefully();
        sprintf(response,"%d,Playlist Stopping Gracefully,,,,,,,,,,\n",COMMAND_SUCCESS);
      }
      else
      {
        sprintf(response,"%d,Not playing,,,,,,,,,,\n",COMMAND_FAILED);
      }
      break;
    case 'd':
      if(FPPstatus==FPP_STATUS_PLAYLIST_PLAYING || FPPstatus==FPP_STATUS_STOPPING_GRACEFULLY)
      {
        StopPlaylistNow();
        sprintf(response,"%d,Playlist Stopping Now,,,,,,,,,,\n",COMMAND_SUCCESS);
      }
      else
      {
        sprintf(response,"%d,Not playing,,,,,,,,,,\n",COMMAND_FAILED);
      }
      break;
    case 'R':
			LoadScheduleFromFile();
      sprintf(response,"%d,Reloading Schedule,,,,,,,,,,\n",COMMAND_SUCCESS);
      break;

    case 'v':
      s = strtok(command,",");
      s = strtok(NULL,",");
			strcpy(MPG123volume,s);
			MPG_SetVolume(MPG123volume);
      sprintf(response,"%d,Setting Volume,,,,,,,,,,\n",COMMAND_SUCCESS);
      break;


    }
    bytes_sent = sendto(socket_fd, response, strlen(response), 0,
                          (struct sockaddr *) &(client_address), sizeof(struct sockaddr_un));
     //LogWrite(response);
  }


  void exit_handler(int signum)
	{
     LogWrite("Caught signal %d\n",signum);
     CloseCommand();
	   exit(signum);
	}

