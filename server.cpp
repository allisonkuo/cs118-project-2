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
	if((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
	{
		perror("cannot create socket");
		return 0;
	}

	printf("created socket: descriptor=%d\n",fd);

	memset((void *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(atoi(argv[1]));
	cout << "sin_port: " << myaddr.sin_port << endl;

	if(bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0)
	{
	    perror("bind failed");
	    return 0;
	}

	addr_len = sizeof(myaddr);
	if(getsockname(fd, (struct sockaddr *)&myaddr, &addr_len) < 0)
	{
	    perror("getsockanem failed");
	    return 0;
	}

	printf("bind complete. Port number = %d\n", ntohs(myaddr.sin_port));
	
	for(;;)
	{
		printf("waiting\n");
		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
		printf("received %d bytes\n", recvlen);
		if(recvlen > 0)
		{
			buf[recvlen] = 0;
			printf("received message: \"%s\"\n",buf);
		}
	}
	exit(0);
}
