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

    struct hostent *hp; /* host information */ 
    struct sockaddr_in servaddr; /* server address */ 
    char *request_message = "request!"; /* fill in the server's address and data */ 
    char *ack_message = "1";
    memset((char*)&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(atoi(argv[2])); /* look up the address of the server given its name */ 
    
    unsigned char buf[BUFSIZE];
    if((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
    {
	perror("cannot create socket");
	return 0;
    }
    hp = gethostbyname(argv[1]); 
    if (!hp) 
    { 
	fprintf(stderr, "could not obtain address of %s\n", argv[1]); 
	return 0; 
    } /* put the host's address into the server address structure */ 
    memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length); /* send a message to the server */ 
    if (sendto(fd, request_message, strlen(request_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
	perror("sendto failed"); 
	return 0; 
    }

    for(;;)
    {
	addrlen = sizeof(servaddr);
	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
	printf("received message: \"%s\"\n",buf);
	
	if (sendto(fd, ack_message, strlen(ack_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
	{ 
	    perror("send to failed"); 
	    return 0; 
	}
    }
}
