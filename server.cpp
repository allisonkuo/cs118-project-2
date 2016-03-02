#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>

using namespace std;

int main(int argc,char *argv[])
{
    	struct sockaddr_in myaddr;
	int fd;
	unsigned int addr_len;
	if((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
	{
		perror("cannot create socket");
		return 0;
	}

	printf("created socket: descriptor=%d\n",fd);

	memset((void *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = atoi(argv[1]);
	cout << "sin_port" << myaddr.sin_port << endl;

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
	exit(0);
}
