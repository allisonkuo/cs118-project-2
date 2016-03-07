#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE 2048
#define HEADERSIZE 5000
#define TIMEOUT 5

//GLOBAL VARIABLES
int fd; //socket
struct sockaddr_in myaddr, cliaddr;
socklen_t addrlen;
char *ack;
char **packet_contents;
time_t *timestamps;
int window_size;

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

void *listenthread(void *fp)
{
    unsigned char received_buf[BUFSIZE];
    int recvlen = -1;
    while(1)
    {
	memset(received_buf,0,sizeof(received_buf));
	// listen for receiver request/ACKs
	printf("waiting\n");
	recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
	printf("received %d bytes\n", recvlen);
	if(recvlen < 3)
	{
	    printf("error receiving");
	}

	received_buf[recvlen] = 0;
	// send message based off request
	if(received_buf[0] == 'A' && received_buf[1] == 'C' && received_buf[2] == 'K')
	{
	    printf("received: %s\n",received_buf);
	    char* ack_pos = strstr((char*) received_buf, "ACK: ") + 5;
	    int i;
	    for(i = 0; ; i++)
	    {
		if(!isdigit(ack_pos[i]))
		    break;
	    }
	    char ack_num[30000];
	    strncpy(ack_num, ack_pos, i);
	    ack[atoi(ack_num)] = 1;
	}
    }
}

void *talkthread(void *fp)
{
    // listen for receiver request/ACKs
    // read through file
    unsigned char file_buf[BUFSIZE];
    unsigned char send_buf[BUFSIZE + HEADERSIZE];      
    int seq_num = 0;
    int initial_send_count = 0;

    //initial loop to send window_size number of packets
    while(inital_send_count < window_size || !feof((FILE *)fp))
    { 
	char seq_num_string[50];
	char header[] = "SEQUENCE NUMBER: ";

	// create packet headers
	itoa(seq_num, seq_num_string); 

	memset(send_buf,0,sizeof(send_buf));
	strcat(header, (const char*) seq_num_string); 
	strcat(header, "\n");
	strcpy((char*) send_buf, (const char*) header);

	// place file into buffer
	int read_count;
	memset(file_buf,0,sizeof(file_buf));
	read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp);
	//printf("read_count:%i\n",read_count);
	strcat((char*) send_buf, (const char*) file_buf);
	strcpy(packet_contents[seq_num], (const char*) send_buf);
	//printf("send_buf:\n%s\n\n",send_buf);
	printf("packet_content:\n%s\n", packet_contents[seq_num]);
	if (read_count > 0)
	{
	    // send the file packets
	    int sent_count = sendto(fd, send_buf, strlen((const char*)send_buf), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
	    if (sent_count < 0)
	    {
		perror("error sending file");
		exit(1);
	    }
	    timestamps[seq_num] = time(NULL);
	    printf("time: %d\n", (long long) timestamps[seq_num]);
	    seq_num += 1;
	}
	initial_send_count += 1;
    }

    //main loop that updates window while sending
    while()
    {
	
    }
}


int main(int argc,char *argv[])
{
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
    int seq_num; // sequence number

    //window size
    window_size = atoi(argv[2])/BUFSIZE;
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
	    printf("received message: %s\n",received_buf);
	}

	
	// send message based off request
	if(received_buf[0] != 'A' && received_buf[1] != 'C' && received_buf[2] != 'K')
	{
	    fp = fopen((const char*) received_buf, "r");
	    if(fp == NULL)
		perror("file not found");
	    else
	    {
		fseek(fp, 0L, SEEK_END);
		filesize = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		ack = (char *) malloc(window_size);
		memset(ack,0,window_size);

		packet_contents = (char **) malloc(window_size * sizeof(char*));
		for(int i = 0; i < window_size; i++)
		    packet_contents[i] = (char *) malloc(BUFSIZE);

		timestamps = (time_t *) malloc(window_size * sizeof(time_t*));

		//start threading
		pthread_t threads[2];
		pthread_create(threads, NULL, listenthread, (void *) fp);
		pthread_create(threads+1,NULL, talkthread, (void *) fp);
		pthread_join(threads[0],NULL);
		pthread_join(threads[1],NULL);

		free(ack);
		free(packet_contents);
		free(timestamps);
	    }
	}
	else
	{
	    printf("received ACK\n");
	}
    }

    exit(0);
}
