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

    // send requested file message to the server 
    if (sendto(fd, request, strlen(request), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
	perror("sendto failed"); 
	exit(1); 
    }


    char **file_content;
    int max_packets = 1;
    int received_packets_count = 0;
    int total_packets = -1;
    file_content = (char **) malloc(100 * sizeof(char*));
    int j;
    for(j = 0; j < 100; j++)
    {
	file_content[j] = (char *)malloc(BUFSIZE);
    }
    //main loop
    for(;;)
    {
	if(total_packets == received_packets_count)
	{
	    //received whole message
	}
	// wait for packets from server
	addrlen = sizeof(servaddr);
	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
//	printf("received message: %s\n",buf);

	// parse header
	//sequence number
	char* header_pos = strstr((char*) buf, "SEQUENCE NUMBER: ") + 17;
	int i;
	char sequence_num[30000];
	if(header_pos[0] == '-' && header_pos[1] == '1')
	    i = 2;
	else
	{
	    for(i = 0; ; i++)
	    {
		if(!isdigit(header_pos[i]))
		    break;
	    }
	}
	strncpy(sequence_num, header_pos,i);
	// send ack if received packet
	char ack[30000] = "ACK: ";  // ACK
	strcat(ack, sequence_num);
	if (sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
	{ 
	    perror("send to failed"); 
	    exit(1); 
	}

	//parse out message
	int sequence = atoi(sequence_num);
	char* message_start_position = strstr((char*) buf, "\n") + 1;
	if(sequence == -1)
	{
	   for(i = 0; ; i++)
	   {
		if(!isdigit(header_pos[i]))
		    break;
	   }
	   char total_packets_string[30000];
	   strncpy(total_packets_string, message_start_position, i);
	   total_packets = atoi(total_packets_string);
	   continue;
	}
	// add to message buffer
	while(sequence >= max_packets) //allocate more memory
	{
		file_content = (char **) realloc(file_content, max_packets * 2);
		for(j = max_packets; j < max_packets * 2; j++)
		{
		    file_content[j] = (char *) malloc(BUFSIZE);
		}
		max_packets = max_packets * 2;
	}
   	strcpy(file_content[sequence], message_start_position);
	printf("packet %i\n%s",sequence, file_content[sequence]);
	received_packets_count += 1;
    }
}
