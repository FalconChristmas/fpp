#include "fpp.h"
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

extern char logText[256];
extern int FPPstatus;
extern int FPPDmode;
extern char currentPlaylist[128];
extern char currentPlaylistFile[128];

extern PlaylistEntry playList[32];
extern int playListCount;
extern int currentPlaylistEntry;
extern int nextPlaylistEntry;
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
   sprintf(logText,"Initializing Command Module\n\r");
   LogWrite(logText);
   signal(SIGINT, exit_handler);
   signal(SIGTERM, exit_handler);

   if((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
   {
    perror("server: socket");
   }
	 
   fcntl(socket_fd, F_SETFL, O_NONBLOCK);
   memset(&server_address, 0, sizeof(server_address));
   server_address.sun_family = AF_UNIX;
   strcpy(server_address.sun_path, "/home/pi/bin/FPPD");
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
        sprintf(response,"%d,%d,%s,%s\n",FPPDmode,FPPstatus,NextPlaylist,NextScheduleStartText);
      }
      else
      {
				if(playList[currentPlaylistEntry].cType == 'b' || playList[currentPlaylistEntry].cType == 'm')
				{
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",FPPDmode,FPPstatus,MPG123volume,currentPlaylist,playList[currentPlaylistEntry].cType,
																			 playList[currentPlaylistEntry].seqName,playList[currentPlaylistEntry].songName,
																			 currentPlaylistEntry+1,playListCount,(int)mpg123.seconds,(int)mpg123.secondsleft,
																			 NextPlaylist,NextScheduleStartText);
				}
				else if (playList[currentPlaylistEntry].cType == 's')
				{
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",FPPDmode,FPPstatus,MPG123volume,currentPlaylist,playList[currentPlaylistEntry].cType,
																			 playList[currentPlaylistEntry].seqName,playList[currentPlaylistEntry].songName,
																			 currentPlaylistEntry+1,playListCount,E131secondsElasped,E131secondsRemaining,
				  														 NextPlaylist,NextScheduleStartText);
				}
				else
				{
					sprintf(response,"%d,%d,%s,%s,%c,%s,%s,%d,%d,%d,%d,%s,%s\n",FPPDmode,FPPstatus,MPG123volume,currentPlaylist,playList[currentPlaylistEntry].cType,
																			 playList[currentPlaylistEntry].seqName,playList[currentPlaylistEntry].songName,	
																			 currentPlaylistEntry+1,playListCount,numberOfSecondsPaused,
																			 (int)playList[currentPlaylistEntry].pauselength-numberOfSecondsPaused, NextPlaylist,NextScheduleStartText);
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
			printf("parse1=%s\n",s);
      s = strtok(NULL,",");
			printf("parse2=%s\n",s);
      strcpy(currentPlaylistFile,s);
      s = strtok(NULL,",");
			printf("parse3=%s\n",s);
		  currentPlaylistEntry = atoi(s);
  		nextPlaylistEntry=currentPlaylistEntry;
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
  		printf("parse1=%s\n",s);
      s = strtok(NULL,",");
  		printf("parse2=%s\n",s);
      strcpy(currentPlaylistFile,s);
      s = strtok(NULL,",");
			printf("parse3=%s\n",s);
		  currentPlaylistEntry = atoi(s);
  		nextPlaylistEntry=currentPlaylistEntry;

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
     sprintf(logText,"Caught signal %d\n",signum);
     LogWrite(logText);
     CloseCommand();
	   exit(signum);
	}

