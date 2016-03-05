#include <sys/types.h> 
#include <sys/socket.h> 
#include <stdio.h> /* for fprintf */ 
#include <string.h> /* for memcpy */ 
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#define BUFSIZE 5000 // MAKE THE SAME SIZE AS MAX SENDER'S PACKET SIZE

int main(int argc,char *argv[])
{
    int fd, recvlen;
    unsigned int addrlen;

    struct hostent *hp; // host information 
    struct sockaddr_in servaddr; // server address

    char *request = argv[3]; // request message

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

	// parse header
	char* sequence_num_pos = strstr((char*) buf, "SEQUENCE NUMBER: ") + 17;
	int i;
	for(i = 0; ; i++)
	{
	    if(!isdigit(sequence_num_pos[i]))
		break;
	}
	char sequence_num[30000];
	strncpy(sequence_num, sequence_num_pos, i);

	// send ack if received packet
	char ack[30000] = "ACK: ";  // ACK
	strcat(ack, sequence_num);
	if (sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
	{ 
	    perror("send to failed"); 
	    exit(1); 
	}
    }
}
