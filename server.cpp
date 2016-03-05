#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>

#define BUFSIZE 20000

using namespace std;

int main(int argc,char *argv[])
{
  struct sockaddr_in myaddr, cliaddr;
  int fd;
  unsigned int addr_len;

  unsigned char buf[BUFSIZE];
  socklen_t addrlen = sizeof(cliaddr);
  int recvlen;

  // open socket
  if((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
  {
    perror("cannot create socket");
    exit(1);
  }

  // setting socket properties
  memset((void *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(atoi(argv[1]));

  // binding socket
  if(bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
  {
    perror("bind failed");
    exit(1);
  }

  // get socket name
  addr_len = sizeof(myaddr);
  if(getsockname(fd, (struct sockaddr *)&myaddr, &addr_len) < 0)
  {
    perror("getting socket name failed");
    exit(1);
  }

  printf("bind complete. Port number = %d\n", ntohs(myaddr.sin_port));


  FILE *fp;
  unsigned char received_buf[BUFSIZE];
  unsigned char file_buf[BUFSIZE];
  char *send_buf;
  int filesize;
  // other stuff
  for(;;)
  {
    // listen for receiver request/ACKs
    printf("waiting\n");
    recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
    printf("received %d bytes\n", recvlen);
    if(recvlen > 0)
    {
      received_buf[recvlen] = 0;
      printf("received message: \"%s\"\n",received_buf);
    }

    // send message again
    if(strcmp((const char*) received_buf, "ACK") != 0)
    {
      fp = fopen((const char*) received_buf, "r");
      if(fp == NULL)
      {
        perror("file not found");
      }
      else
      {
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        while(!feof(fp))
        {
          int read_count;
          read_count = fread(file_buf,1,sizeof(file_buf),fp);
          if (read_count > 0)
          {
            int sent_count = sendto(fd, file_buf, strlen((const char*)file_buf), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            printf("%d\n", strlen((const char*)file_buf));
            if (sent_count < 0)
            {
              perror("error sending file stupid");
              exit(1);
            }
            /*unsigned char* send_buf;
              send_buf = file_buf;
              do {
              int sent_count;
              if (sent_count < 0)
              perror("error sending file");
              send_buf += sent_count;
              read_count -= sent_count;
              } while (read_count > 0);*/
          }
        }
      }
    }
    else
    {
      printf("received ACK\n");
    }
  }

  exit(0);
}
