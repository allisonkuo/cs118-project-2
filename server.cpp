#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>

#define BUFSIZE 2048

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
	
  // other stuff
	for(;;)
	{
	  char *response = "here is your file";
		
    // listen for receiver request/ACKs
    printf("waiting\n");
		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
		printf("received %d bytes\n", recvlen);
		if(recvlen > 0)
		{
			buf[recvlen] = 0;
			printf("received message: \"%s\"\n",buf);
		}

    // send message again
		if(atoi((const char*) buf) != 1)
		{
		sendto(fd, response, strlen(response), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
		}
	}

	exit(0);
}
