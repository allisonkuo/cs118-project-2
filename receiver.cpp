#include <sys/types.h> 
#include <sys/socket.h> 
#include <stdio.h> /* for fprintf */ 
#include <string.h> /* for memcpy */ 
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 2048
int main(int argc,char *argv[])
{
  int fd, recvlen;
  unsigned int addrlen;

  struct hostent *hp; // host information 
  struct sockaddr_in servaddr; // server address
 
  char *request = "request!"; // request message
  char *ack = "1";  // ACK

  // set server address information
  memset((char*)&servaddr, 0, sizeof(servaddr)); 
  servaddr.sin_family = AF_INET; 
  servaddr.sin_port = htons(atoi(argv[2])); // look up the address of the server given its name

  // create socket 
  unsigned char buf[BUFSIZE];
  if((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
  {
    perror("cannot create socket");
    exit(1);
  }

  // get server's host name
  hp = gethostbyname(argv[1]); 
  if (!hp) 
  { 
    fprintf(stderr, "could not obtain address of %s\n", argv[1]); 
    exit(1); 
  } 

  // put the host's address into the server address structure 
  memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length); 

  // send a message to the server 
  if (sendto(fd, request, strlen(request), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
  { 
    perror("sendto failed"); 
    exit(1); 
  }

  for(;;)
  {
    // wait for packets from server
    addrlen = sizeof(servaddr);
    recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
    printf("received message: \"%s\"\n",buf);

    // send ack if received packet
    if (sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
      perror("send to failed"); 
      exit(1); 
    }
  }
}
