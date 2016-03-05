#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

using namespace std;

#define BUFSIZE 5000
#define HEADERSIZE 5000

// simple function to convert integer to ascii
void itoa (int n, char s[]) 
{
  int i, sign;
  if ((sign = n) < 0)
    n = -n;
  i = 0;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  
  // reverse the string
  int j, k;
  char c;
  for (j = 0, k = strlen(s) - 1; j < k; j++, k--)
  {
    c = s[j];
    s[j] = s[k];
    s[k] = c;
  }
}


int main(int argc,char *argv[])
{
  struct sockaddr_in myaddr, cliaddr;
  int fd;
  unsigned int addr_len;
  unsigned char buf[BUFSIZE];
  socklen_t addrlen = sizeof(cliaddr);

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
  int recvlen;
 
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
          int temp = 15234;
          char temp_string[50];
          unsigned char send_buf[BUFSIZE + HEADERSIZE];
          itoa(temp, temp_string);
          char header[] = "SEQUENCE NUMBER: ";
          strcat(header, (const char*) temp_string); 
          printf("%s\n", header);
          strcpy((char*) send_buf, (const char*) header);
          
          int read_count;
          read_count = fread(file_buf,1,sizeof(file_buf),fp);
          strcat((char*) send_buf, (const char*) file_buf);
          if (read_count > 0)
          {
            int sent_count = sendto(fd, file_buf, strlen((const char*)file_buf), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
            
            if (sent_count < 0)
            {
              perror("error sending file");
              exit(1);
            }
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
