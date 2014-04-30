/*
 *   Falcon Pi Player (FPP) 
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
#include <stdlib.h>
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
    // Set new log level - "fpp --log-level info"   "fpp --log-level debug"
    else if((strcmp(argv[1],"--log-level") == 0) &&  argc > 2)
    {
      sprintf(command,"LogLevel,%s,", argv[2]);
      SendCommand(command);
    }
    // Set new log mask - "fpp --log-mask channel,mediaout"   "fpp --log-mask all"
    else if((strcmp(argv[1],"--log-mask") == 0) &&  argc > 2)
    {
      char newMask[128];
      strcpy(newMask, argv[2]);

      char *s = strchr(argv[2], ',');
      while (s) {
        *s = ';';
        s = strchr(s+1, ',');
      }

      sprintf(command,"LogMask,%s,", newMask);
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

