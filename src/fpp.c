#include "fpp.h"
#include "log.h"
#include "command.h"
#include "sequence.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int socket_fd;
struct sockaddr_un server_address;
struct sockaddr_un client_address;
int bytes_received, bytes_sent;

char command[128];
char response[256];

void SendCommand(const char * com);
void SetupDomainSocket(void);

socklen_t address_length;

int main (int argc, char *argv[])
{
  SetupDomainSocket();
  if(argc>1)
  {
    // Status - example "fpp -s"
    if(strncmp(argv[1],"-s",2)==0)
    {
      SendCommand("s");
    }
    // Set Volume - example "fpp -v 50"
    else if(strncmp(argv[1],"-v",2)==0)
    {
      sprintf(command,"v,%s,",argv[2]);
      SendCommand(command);
    }
    // Play Playlist - example "fpp -p playlistFile"
    else if((strncmp(argv[1],"-p",2) == 0) &&  argc > 2)
    {
      sprintf(command,"p,%s,",argv[2]);
      SendCommand(command);
    }
    // Play Playlist only once- example "fpp -P playlistFile 1"
    else if((strncmp(argv[1],"-P",2) == 0) &&  argc > 2)
    {
      sprintf(command,"P,%s,%s,",argv[2],argv[3]);
      SendCommand(command);
    }
    // Stop gracefully - example "fpp -S"
    else if(strncmp(argv[1],"-S",2) == 0)
    {
      sprintf(command,"S");
      SendCommand(command);
    }
    // Stop now - example "fpp -d"
    else if(strncmp(argv[1],"-d",2) == 0)
    {
      sprintf(command,"d");
      SendCommand(command);
    }
    // Shutdown fppd daemon
    else if(strncmp(argv[1],"-q",2) == 0)
    {
      sprintf(command,"q");
      SendCommand(command);
    }
    // Reload schedule example "fpp -R"
    else if(strncmp(argv[1],"-R",2) == 0)
    {
      sprintf(command,"R");
      SendCommand(command);
    }
    else if(strncmp(argv[1],"-w",2) == 0)
    {
      sprintf(command,"w");
      SendCommand(command);
    }
    else if(strncmp(argv[1],"-r",2) == 0)
    {
      sprintf(command,"r");
      SendCommand(command);
    }
    // Start an effect - example "fpp -e effectName"
    else if((strncmp(argv[1],"-e",2) == 0) &&  argc > 2)
    {
      if (strchr(argv[2], ','))
        sprintf(command,"e,%s,",argv[2]);
      else
        sprintf(command,"e,%s,1,",argv[2]);
      SendCommand(command);
    }
    // Trigger an event - example "fpp -t eventName"
    else if((strncmp(argv[1],"-t",2) == 0) &&  argc > 2)
    {
      sprintf(command,"t,%s,",argv[2]);
      SendCommand(command);
    }
    // Set new log level - "fpp -ll info"   "fpp -l debug"
    else if((strncmp(argv[1],"-ll",3) == 0) &&  argc > 2)
    {
	  int newLevel = 0;

      if (!strcmp(argv[2], "warn")) {
	  	newLevel = LOG_WARN;
      } else if (!strcmp(argv[2], "debug")) {
	  	newLevel = LOG_DEBUG;
      } else {
	  	newLevel = LOG_INFO;
      }

      sprintf(command,"LogLevel,%d,", newLevel);
      SendCommand(command);
    }
    // Set new log mask - "fpp -lm channel,mediaout"   "fpp -l all"
    else if((strncmp(argv[1],"-lm",3) == 0) &&  argc > 2)
    {
      int newMask = 0;
      char *s = NULL;

      s = strtok(argv[2], ",");
      while (s) {
        if (!strcmp(s, "none")) {
          newMask = VB_NONE;
        } else if (!strcmp(s, "all")) {
          newMask = VB_ALL;
        } else if (!strcmp(s, "generic")) {
          newMask |= VB_GENERIC;
        } else if (!strcmp(s, "channelout")) {
          newMask |= VB_CHANNELOUT;
        } else if (!strcmp(s, "channeldata")) {
          newMask |= VB_CHANNELDATA;
        } else if (!strcmp(s, "command")) {
          newMask |= VB_COMMAND;
        } else if (!strcmp(s, "e131bridge")) {
          newMask |= VB_E131BRIDGE;
        } else if (!strcmp(s, "effect")) {
          newMask |= VB_EFFECT;
        } else if (!strcmp(s, "event")) {
          newMask |= VB_EVENT;
        } else if (!strcmp(s, "mediaout")) {
          newMask |= VB_MEDIAOUT;
        } else if (!strcmp(s, "playlist")) {
          newMask |= VB_PLAYLIST;
        } else if (!strcmp(s, "schedule")) {
          newMask |= VB_SCHEDULE;
        } else if (!strcmp(s, "sequence")) {
          newMask |= VB_SEQUENCE;
        } else if (!strcmp(s, "setting")) {
          newMask |= VB_SETTING;
        }

        s = strtok(NULL,",");
      }

      sprintf(command,"LogMask,%d,", newMask);
      SendCommand(command);
    }
    else
    {
    }
  }
  else
  {
    return 0;
  }

  return 0;
}

void SetupDomainSocket(void)
{
 if((socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
 {
  perror("client: socket");
  return;
 }

 memset(&client_address, 0, sizeof(struct sockaddr_un));
 client_address.sun_family = AF_UNIX;
 strcpy(client_address.sun_path, FPP_CLIENT_SOCKET);

 unlink(FPP_CLIENT_SOCKET);
 if(bind(socket_fd, (const struct sockaddr *) &client_address,
         sizeof(struct sockaddr_un)) < 0)
 {
  perror("client: bind");
  return;
 }
 memset(&server_address, 0, sizeof(struct sockaddr_un));
 server_address.sun_family = AF_UNIX;
 strcpy(server_address.sun_path, FPP_SERVER_SOCKET);
}

void SendCommand(const char * com)
{
 int max_timeout = 1000;
 int i=0;
 bytes_sent = sendto(socket_fd, com, strlen(com), 0,
                     (struct sockaddr *) &server_address,
                     sizeof(struct sockaddr_un));

 address_length = sizeof(struct sockaddr_un);
 while(i < max_timeout)
 {
 	 i++;
 	 bytes_received = recvfrom(socket_fd, response, 256, MSG_DONTWAIT,
                           (struct sockaddr *) &(server_address),
                           &address_length);
 	 if(bytes_received > 0)
	 {
		 break;
	 }
	 usleep(500);
 }
 
 close(socket_fd);
 unlink(FPP_CLIENT_SOCKET);
 if(bytes_received > 0)
 {
  printf("%s",response);
 }
 else
 {
 	 printf("false");
 }
}

