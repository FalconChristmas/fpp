#include "command.h"
#include <sys/socket.h>
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
    // Play Playlist - example "fpp -p playlistFile"
    else if((strncmp(argv[1],"-p",2) == 0) &&  argc > 2)
    {
      sprintf(command,"p,%s,",argv[2]);
      SendCommand(command);
    }
    // Play Playlist only once- example "fpp -P playlistFile"
    else if((strncmp(argv[1],"-P",2) == 0) &&  argc > 2)
    {
      sprintf(command,"P,%s,",argv[2]);
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
    // Reload schedule example "fpp -R"
    else if(strncmp(argv[1],"-R",2) == 0)
    {
      sprintf(command,"R");
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
 strcpy(client_address.sun_path, "/tmp/FPP");

 unlink("/tmp/FPP");
 if(bind(socket_fd, (const struct sockaddr *) &client_address,
         sizeof(struct sockaddr_un)) < 0)
 {
  perror("client: bind");
  return;
 }
 memset(&server_address, 0, sizeof(struct sockaddr_un));
 server_address.sun_family = AF_UNIX;
 strcpy(server_address.sun_path, "/tmp/FPPD");
}

void SendCommand(const char * com)
{
 int max_timeout = 1000;
 int i=0;
// printf("Sending = %s\n",com);
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
 if(bytes_received > 0)
 {
  printf("%s",response);
 }
 else
 {
 	 printf("false");
 }
}
