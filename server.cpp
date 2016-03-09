#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE 948
#define HEADERSIZE 52
#define TIMEOUT 2
#define SRAND 3
#define MAXWINDOW 64//multiples of 2
#define RTT 2
//GLOBAL VARIABLES
int fd; //socket
struct sockaddr_in myaddr, cliaddr;
socklen_t addrlen;
char *ack;
char **packet_contents;
time_t *timestamps;
int window_size;
pthread_mutex_t lock;
int window_start;
char final_ack;
double loss_rate;
double corruption_rate;

int mode; //0: without extra features, 1: Slow start
int threshhold_reached;


// function to determine if packet is lost
int loss_check ()
{
    int random = rand() % 1000;
    if (random < loss_rate * 1000)
    {
	return 1; //lost
    }
    else
    {
	return 0; //not lost
    }
}

// function to determine if packet is corrupted
int corruption_check ()
{
    int random = rand() % 1000;
    if (random < corruption_rate * 1000)
    {
	return 1; //corrupted
    }
    else
    {
	return 0; //not corrupted
    }
}

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
	printf("\nwaiting\n");
	recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);

	if(loss_check())
	{
	    printf("LOST PACKET\n");
	    continue;
	}
	if(corruption_check())
	{
	    printf("CORRUPTED PACKET\n");
	    continue;
	}

	//printf("received %d bytes\n", recvlen);
	if(recvlen < 3)
	    printf("error receiving");

	received_buf[recvlen] = 0;

	// send message based off request
	if(received_buf[0] == 'A' && received_buf[1] == 'C' && received_buf[2] == 'K')
	{
	    char* ack_pos = strstr((char*) received_buf, "ACK: ") + 5;
	    int i;
	    char ack_num[30000];
	    memset(ack_num,0,sizeof(ack_num));
	    if(ack_pos[0] == '-' && ack_pos[1] == '1')
	    {
		final_ack = 1;
	    }
	    else
	    {
		for(i = 0; ; i++)
		{
		    if(!isdigit(ack_pos[i]))
			break;
		}
		strncpy(ack_num, ack_pos, i);
		pthread_mutex_lock(&lock);
		/*int index = atoi(ack_num) - window_start;
		  if(index < 0)
		  index += 30;
		  index = index % 5;*/
		int index = atoi(ack_num) % 30;

		printf("RECEIVED: ACK %d\n", index);
		ack[atoi(ack_num) - window_start] = 1;
		pthread_mutex_unlock(&lock);
	    }
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
    int packet_num = 0;
    int initial_send_count = 0;

    //initial loop to send window_size number of packets
    while(initial_send_count < window_size && !feof((FILE *)fp))
    { 
	char packet_num_string[50];
	char seq_num_string[50];
	char header[] = "PACKET NUMBER: ";
	char header2[] = "SEQUENCE NUMBER: ";
	if(seq_num >= 30)
	    seq_num = 0;

	// create packet headers
	itoa(packet_num, packet_num_string);
	itoa(seq_num, seq_num_string); 

	memset(send_buf,0,sizeof(send_buf));
	strcpy((char*) send_buf, (const char*) header);
	if (packet_num < 10)
	    strcat((char*) send_buf, "00000");
	else if (packet_num < 100)
	    strcat((char*) send_buf, "0000");
	else if (packet_num < 1000)
	    strcat((char*) send_buf, "000");
	else if (packet_num < 10000)
	    strcat((char*) send_buf, "00");
	else if (packet_num < 100000)
	    strcat((char*) send_buf, "0");

	strcat((char*) send_buf, (const char*) packet_num_string);
	strcat((char*) send_buf, "; ");

	strcat((char*) send_buf, (const char*) header2);
	if(seq_num < 10)
	{
	    strcat((char*) send_buf, "0");
	}
	strcat((char*) send_buf, (const char*) seq_num_string); 
	strcat((char*) send_buf, "\n");

	// place file into buffer
	int read_count;
	memset(file_buf,0,sizeof(file_buf));
	read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp);
	strcat((char*) send_buf, (const char*) file_buf);
	strcpy(packet_contents[seq_num], (const char*) send_buf);
	if (read_count > 0)
	{
	    // send the file packets
	    int sent_count = sendto(fd, send_buf, strlen((const char*)send_buf), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
	    if (sent_count < 0)
	    {
		perror("error sending file");
		exit(1);
	    }
	    printf("SENT SEQUENCE NUM: %i\n",seq_num);
	    timestamps[seq_num] = time(NULL);
	    printf("TIMESTAMP OF #%d: %d\n", seq_num, (int)timestamps[seq_num]);

	    packet_num += 1;
	    seq_num += 1;
	}
	initial_send_count += 1;
    }
    //main loop that updates window while sending
    while(1)
    {
	int num_loops = window_size;
	if(initial_send_count < window_size)
	{
	    // make sure works with fewer packets than window size
	    num_loops = initial_send_count;
	}
	int i;
	// check for ACKS for all packets sent in window
	for (i = 0; i < num_loops; i++)
	{
	    pthread_mutex_lock(&lock);
	    //check for timeouts
	    if (ack[i] == 0)
	    {
		pthread_mutex_unlock(&lock);
		// resend if TIMEOUT
		if (difftime(time(NULL), timestamps[i]) >= TIMEOUT)
		{
		    int sent_count = sendto(fd, packet_contents[i], strlen((const char*) packet_contents[i]), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
		    if (sent_count < 0)
			perror("error sending file");          

		    printf("TIMED OUT\n");
		    printf("RETRANSMITTING SEQUENCE NUM: %d\n", i + window_start % 30);

		    // update timestamp of resent message
		    timestamps[i] = time(NULL);
		    printf("TIMESTAMP OF #%d: %d\n", i + window_start % 30, (int)timestamps[i]);
		}
	    }
	    else
		pthread_mutex_unlock(&lock);
	}

	while(1)
	{
	    pthread_mutex_lock(&lock);
	    if(ack[0] == 1 && !feof((FILE *)fp))
	    {
		// move window
		window_start += 1;
		for(int i = 0; i < window_size - 1; i++)
		{
		    ack[i] = ack[i+1];
		    timestamps[i] = timestamps[i+1];
		    memset(packet_contents[i],0,sizeof(packet_contents[i]));
		    strcpy(packet_contents[i],packet_contents[i + 1]);
		    //packet_contents[i] = packet_contents[i + 1];
		}
		ack[window_size - 1] = 0;

		pthread_mutex_unlock(&lock);
		// make the correct header
		char packet_num_string[50];
		char seq_num_string[50];
		char header[] = "PACKET NUMBER: ";
		char header2[] = "SEQUENCE NUMBER: ";

		// update packet headers
		itoa(window_start + window_size - 1, packet_num_string);
		itoa((window_start % 30 + window_size - 1) % 30, seq_num_string); 

		// reset the buffers        
		memset(send_buf,0,sizeof(send_buf));
		memset(file_buf,0,sizeof(file_buf));


		strcpy((char*) send_buf, (const char*) header);
		strcpy((char*) send_buf, (const char*) header);
		if (packet_num < 10)
		    strcat((char*) send_buf, "00000");
		else if (packet_num < 100)
		    strcat((char*) send_buf, "0000");
		else if (packet_num < 1000)
		    strcat((char*) send_buf, "000");
		else if (packet_num < 10000)
		    strcat((char*) send_buf, "00");
		else if (packet_num < 100000)
		    strcat((char*) send_buf, "0");
		strcat((char*) send_buf, (const char*) packet_num_string);
		strcat((char*) send_buf, "; ");
		strcat((char*) send_buf, (const char*) header2);
		if(atoi(seq_num_string) < 10)
		{
		    strcat((char *)send_buf,"0");
		}
		strcat((char *) send_buf, (const char*) seq_num_string); 
		strcat((char *)send_buf, "\n");

		//read/save next packet
		int read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp); 
		strcat((char*) send_buf, (const char*) file_buf);
		memset(packet_contents[window_size - 1],0,sizeof(packet_contents[window_size - 1]));
		strcpy(packet_contents[window_size - 1], (const char*) send_buf);

		// send next packet in window
		int sent_count = sendto(fd, packet_contents[window_size - 1], strlen((const char*) packet_contents[window_size - 1]), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));

		printf("SENT SEQUENCE NUM: %s\n", seq_num_string);

		if (sent_count < 0)
		    perror("error sending file\n");

		timestamps[window_size - 1] = time(NULL);
		printf("TIMESTAMP OF #%s: %d\n", seq_num_string, (int)timestamps[window_size - 1]);
	    }
	    else
	    {
		pthread_mutex_unlock(&lock);
		break;
	    }
	}
	//check if we have received all ACK
	if(feof((FILE *)fp))
	{
	    int all_sent = 1;
	    pthread_mutex_lock(&lock);
	    for(int i = 0; i < window_size; i++)
	    {
		if(ack[i] == 0)
		    all_sent = 0;
	    }
	    pthread_mutex_unlock(&lock);
	    //send final message and check for ACK
	    if(all_sent)
	    {
		int first_run = 1;
		time_t final_timestamp = time(NULL);
		while(final_ack == 0)
		{
		    if (difftime(time(NULL), final_timestamp) >= TIMEOUT || first_run == 1)
		    {
			first_run = 0;
			// make the correct header
			char seq_num_string[50];
			char header[] = "PACKET NUMBER: ";

			// update packet headers
			itoa(-1, seq_num_string); 
			memset(send_buf,0,sizeof(send_buf));
			strcat(header, (const char*) seq_num_string); 
			strcat(header, "\n");
			strcpy((char*) send_buf, (const char*) header);

			//read/save next packet
			char temp_total_packets[30000];

			//TESTING
			//printf("window_start %d\n", window_start);
			//printf("window_size %d\n", window_size);

			itoa(num_loops + window_start, temp_total_packets);
			strcat((char*) send_buf, (const char*) temp_total_packets);

			//printf("buffer:\n%s\n",send_buf);

			// send next packet in window
			int sent_count = sendto(fd, send_buf, strlen((const char*) send_buf), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));
			if (sent_count < 0)
			    perror("error sending file\n");

			final_timestamp = time(NULL);
		    }
		}
	    }
	}
    }
}


void *sslistenthread(void *fp)
{
    unsigned char received_buf[BUFSIZE];
    int recvlen = -1;
    while(1)
    {
	memset(received_buf,0,sizeof(received_buf));
	// listen for receiver request/ACKs
	printf("\nwaiting\n");
	recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);

	if(loss_check())
	{
	    threshhold_reached = 1;
	    printf("LOST PACKET\n");
	    continue;
	}
	if(corruption_check())
	{
	    printf("CORRUPTED PACKET\n");
	    continue;
	}

	//printf("received %d bytes\n", recvlen);
	if(recvlen < 3)
	    printf("error receiving");

	received_buf[recvlen] = 0;

	// send message based off request
	if(received_buf[0] == 'A' && received_buf[1] == 'C' && received_buf[2] == 'K')
	{
	    if(window_size == MAXWINDOW)
	    {
		threshhold_reached = 1;
	    }
	    if(threshhold_reached == 0)
	    {
		pthread_mutex_lock(&lock);
		ack = (char *) realloc(ack, window_size * 2);
		packet_contents = (char **) realloc(packet_contents, window_size * 2 * sizeof(char*));
		for(int i = window_size; i < window_size * 2; i++)
		{
		    ack[i] = 2;
		    packet_contents[i] = (char *) malloc(BUFSIZE + HEADERSIZE);
		    memset(packet_contents[i],0, sizeof(packet_contents[i]));
		}

		timestamps = (time_t *) realloc(timestamps, window_size * 2 * sizeof(time_t));
		window_size *= 2;
		pthread_mutex_unlock(&lock);
	    }
	    else if(window_size < MAXWINDOW)
	    {
		pthread_mutex_lock(&lock);
		ack = (char *) realloc(ack, window_size + 1);
		packet_contents = (char **) realloc(packet_contents, (window_size + 1) * sizeof(char*));
		for(int i = window_size; i < window_size + 1; i++)
		{
		    ack[i] = 2;
		    packet_contents[i] = (char *) malloc(BUFSIZE + HEADERSIZE);
		    memset(packet_contents[i],0, sizeof(packet_contents[i]));
		}

		timestamps = (time_t *) realloc(timestamps, (window_size + 1) * sizeof(time_t));
		window_size += 1;
		pthread_mutex_unlock(&lock);
		printf("WINDOW SIZE: %d\n", window_size);
	    }
	}
	char* ack_pos = strstr((char*) received_buf, "ACK: ") + 5;
	int i;
	char ack_num[30000];
	memset(ack_num,0,sizeof(ack_num));
	if(ack_pos[0] == '-' && ack_pos[1] == '1')
	{
	    final_ack = 1;
	}
	else
	{
	    for(i = 0; ; i++)
	    {
		if(!isdigit(ack_pos[i]))
		    break;
	    }
	    strncpy(ack_num, ack_pos, i);
	    pthread_mutex_lock(&lock);
	    /*int index = atoi(ack_num) - window_start;
	      if(index < 0)
	      index += 30;
	      index = index % 5;*/
	    int index = atoi(ack_num) % 30;

	    printf("RECEIVED: ACK %d\n", index);
	    ack[atoi(ack_num) - window_start] = 1;
	    pthread_mutex_unlock(&lock);
	}
    }
}

void *sstalkthread(void *fp)
{

    // listen for receiver request/ACKs
    // read through file
    unsigned char file_buf[BUFSIZE];
    unsigned char send_buf[BUFSIZE + HEADERSIZE];      
    int seq_num = 0;
    int packet_num = 0;
    int initial_send_count = 0;

    int extra_packets = 0;
    //initial loop to send window_size number of packets
    while(initial_send_count < window_size && !feof((FILE *)fp))
    { 
	char packet_num_string[50];
	char seq_num_string[50];
	char header[] = "PACKET NUMBER: ";
	char header2[] = "SEQUENCE NUMBER: ";
	if(seq_num >= 30)
	    seq_num = 0;

	// create packet headers
	itoa(packet_num, packet_num_string);
	itoa(seq_num, seq_num_string); 

	memset(send_buf,0,sizeof(send_buf));
	strcpy((char*) send_buf, (const char*) header);
	if (packet_num < 10)
	    strcat((char*) send_buf, "00000");
	else if (packet_num < 100)
	    strcat((char*) send_buf, "0000");
	else if (packet_num < 1000)
	    strcat((char*) send_buf, "000");
	else if (packet_num < 10000)
	    strcat((char*) send_buf, "00");
	else if (packet_num < 100000)
	    strcat((char*) send_buf, "0");

	strcat((char*) send_buf, (const char*) packet_num_string);
	strcat((char*) send_buf, "; ");

	strcat((char*) send_buf, (const char*) header2);
	if(seq_num < 10)
	{
	    strcat((char*) send_buf, "0");
	}
	strcat((char*) send_buf, (const char*) seq_num_string); 
	strcat((char*) send_buf, "\n");

	// place file into buffer
	int read_count;
	memset(file_buf,0,sizeof(file_buf));
	read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp);
	strcat((char*) send_buf, (const char*) file_buf);
	strcpy(packet_contents[seq_num], (const char*) send_buf);
	if (read_count > 0)
	{
	    // send the file packets
	    int sent_count = sendto(fd, send_buf, strlen((const char*)send_buf), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
	    if (sent_count < 0)
	    {
		perror("error sending file");
		exit(1);
	    }
	    printf("SENT SEQUENCE NUM: %i\n",seq_num);
	    timestamps[seq_num] = time(NULL);
	    printf("TIMESTAMP OF #%d: %d\n", seq_num, (int)timestamps[seq_num]);

	    packet_num += 1;
	    seq_num += 1;
	}
	initial_send_count += 1;
    }
    //main loop that updates window while sending
    while(1)
    {
	int num_loops = window_size;
	/*    if(initial_send_count < window_size)
	      {
	// make sure works with fewer packets than window size
	num_loops = initial_send_count;
	}*/
	int i;
	// check for ACKS for all packets sent in window
	for (i = 0; i < num_loops; i++)
	{
	    pthread_mutex_lock(&lock);
	    //check for timeouts
	    if (ack[i] == 0)
	    {
		pthread_mutex_unlock(&lock);
		// resend if TIMEOUT
		if (difftime(time(NULL), timestamps[i]) >= TIMEOUT)
		{
		    threshhold_reached = 1;
		    int sent_count = sendto(fd, packet_contents[i], strlen((const char*) packet_contents[i]), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
		    if (sent_count < 0)
			perror("error sending file");          

		    printf("TIMED OUT\n");
		    printf("RETRANSMITTING SEQUENCE NUM: %d\n", i + window_start % 30);

		    // update timestamp of resent message
		    timestamps[i] = time(NULL);
		    printf("TIMESTAMP OF #%d: %d\n", i + window_start % 30, (int)timestamps[i]);
		}
	    }
	    else
		pthread_mutex_unlock(&lock);
	}

	while(1)
	{
	    pthread_mutex_lock(&lock);
	    if(ack[0] == 1 && !feof((FILE *)fp))
	    {
		// move window
		window_start += 1;
		int i;
		for(i = 0; i < window_size - 1; i++)
		{
		    if(ack[i + 1] == 2)
		    {
			ack[i] = 2;
			break;
		    }
		    ack[i] = ack[i+1];
		    timestamps[i] = timestamps[i+1];
		    memset(packet_contents[i],0,sizeof(packet_contents[i]));
		    strcpy(packet_contents[i],packet_contents[i + 1]);
		    //packet_contents[i] = packet_contents[i + 1];
		}
		//ack[window_size - 1] = 0;

		// make the correct header

		for(; i < window_size; i++)
		{
		    char packet_num_string[50];
		    char seq_num_string[50];
		    char header[] = "PACKET NUMBER: ";
		    char header2[] = "SEQUENCE NUMBER: ";

		    // update packet headers
		    itoa(window_start + i, packet_num_string);
		    itoa((window_start % 30 + window_size - 1) % 30, seq_num_string); 

		    printf("window start: %d\n", window_start);
		    printf("i: %d\n", i);
		    printf("ACK: %d\n", ack[i]);
		    // reset the buffers        
		    memset(send_buf,0,sizeof(send_buf));
		    memset(file_buf,0,sizeof(file_buf));


		    strcpy((char*) send_buf, (const char*) header);
		    strcpy((char*) send_buf, (const char*) header);
		    if (packet_num < 10)
			strcat((char*) send_buf, "00000");
		    else if (packet_num < 100)
			strcat((char*) send_buf, "0000");
		    else if (packet_num < 1000)
			strcat((char*) send_buf, "000");
		    else if (packet_num < 10000)
			strcat((char*) send_buf, "00");
		    else if (packet_num < 100000)
			strcat((char*) send_buf, "0");
		    strcat((char*) send_buf, (const char*) packet_num_string);
		    strcat((char*) send_buf, "; ");
		    strcat((char*) send_buf, (const char*) header2);
		    if(atoi(seq_num_string) < 10)
		    {
			strcat((char *)send_buf,"0");
		    }
		    strcat((char *) send_buf, (const char*) seq_num_string); 
		    strcat((char *)send_buf, "\n");

		    //read/save next packet
		    if(feof((FILE *)fp))
		    {
			//ack[i] = 1;
			break;
		    }
		    int read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp); 
		    strcat((char*) send_buf, (const char*) file_buf);
		    memset(packet_contents[i],0,sizeof(packet_contents[i]));
		    strcpy(packet_contents[i], (const char*) send_buf);

		    // send next packet in window
		    int sent_count = sendto(fd, packet_contents[i], strlen((const char*) packet_contents[i]), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));

		    printf("SENT SEQUENCE NUM: %s\n", seq_num_string); 

		    if (sent_count < 0)
			perror("error sending file\n");

		    timestamps[i] = time(NULL);
		    ack[i] = 0;
		    printf("TIMESTAMP OF #%s: %d\n", seq_num_string, (int)timestamps[i]); 
		}
		pthread_mutex_unlock(&lock);
	    }
	    else
	    {
		pthread_mutex_unlock(&lock);
		break;
	    }
	}
	while(1)
	{
	    pthread_mutex_lock(&lock);
	    int two_found = -1;
	    for(int k = 0; k < window_size; k++)
	    {
		if(ack[k] == 2)
		{
		    two_found = k;
		    break;
		}
	    }

	    pthread_mutex_unlock(&lock);
	    //ack[window_size - 1] = 0;
	    if(two_found == -1)
	    {
		break;
	    }
	    // make the correct header
	    for(; two_found < window_size; two_found++)
	    {
		pthread_mutex_lock(&lock);
		char packet_num_string[50];
		char seq_num_string[50];
		char header[] = "PACKET NUMBER: ";
		char header2[] = "SEQUENCE NUMBER: ";

		// update packet headers
		itoa(window_start + two_found, packet_num_string);
		itoa((window_start % 30 + window_size - 1) % 30, seq_num_string); 

		// reset the buffers        
		memset(send_buf,0,sizeof(send_buf));
		memset(file_buf,0,sizeof(file_buf));


		strcpy((char*) send_buf, (const char*) header);
		strcpy((char*) send_buf, (const char*) header);
		if (packet_num < 10)
		    strcat((char*) send_buf, "00000");
		else if (packet_num < 100)
		    strcat((char*) send_buf, "0000");
		else if (packet_num < 1000)
		    strcat((char*) send_buf, "000");
		else if (packet_num < 10000)
		    strcat((char*) send_buf, "00");
		else if (packet_num < 100000)
		    strcat((char*) send_buf, "0");
		strcat((char*) send_buf, (const char*) packet_num_string);
		strcat((char*) send_buf, "; ");
		strcat((char*) send_buf, (const char*) header2);
		if(atoi(seq_num_string) < 10)
		{
		    strcat((char *)send_buf,"0");
		}
		strcat((char *) send_buf, (const char*) seq_num_string); 
		strcat((char *)send_buf, "\n");

		//read/save next packet
		if(feof((FILE *)fp))
		{
		    extra_packets += 1;
		    ack[two_found] = 1;
		    pthread_mutex_unlock(&lock);
		    break;
		}
		int read_count = fread(file_buf,1,sizeof(file_buf)-1,(FILE *)fp); 
		strcat((char*) send_buf, (const char*) file_buf);
		memset(packet_contents[two_found],0,sizeof(packet_contents[two_found]));
		strcpy(packet_contents[two_found], (const char*) send_buf);

		// send next packet in window
		int sent_count = sendto(fd, packet_contents[two_found], strlen((const char*) packet_contents[two_found]), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));

		printf("xSENT SEQUENCE NUM: %s\n", seq_num_string); 

		if (sent_count < 0)
		    perror("error sending file\n");

		timestamps[two_found] = time(NULL);
		ack[two_found] = 0;
		printf("TIMESTAMP OF #%s: %d\n", seq_num_string, (int)timestamps[i]); 

		pthread_mutex_unlock(&lock);
	    }
	}
	//check if we have received all ACK
	if(feof((FILE *)fp))
	{
	    int all_sent = 1;
	    pthread_mutex_lock(&lock);
	    for(int i = 0; i < window_size; i++)
	    {
		if(ack[i] == 0 || ack[i] == 2) 
		    all_sent = 0;
	    }
	    pthread_mutex_unlock(&lock);
	    //send final message and check for ACK
	    if(all_sent)
	    {
		int first_run = 1;
		time_t final_timestamp = time(NULL);
		while(final_ack == 0)
		{
		    if (difftime(time(NULL), final_timestamp) >= TIMEOUT || first_run == 1)
		    {
			first_run = 0;
			// make the correct header
			char seq_num_string[50];
			char header[] = "PACKET NUMBER: ";

			// update packet headers
			itoa(-1, seq_num_string); 
			memset(send_buf,0,sizeof(send_buf));
			strcat(header, (const char*) seq_num_string); 
			strcat(header, "\n");
			strcpy((char*) send_buf, (const char*) header);

			//read/save next packet
			char temp_total_packets[30000];

			//TESTING
			//printf("window_start %d\n", window_start);
			//printf("window_size %d\n", window_size);

			itoa(num_loops + window_start - extra_packets, temp_total_packets);
			strcat((char*) send_buf, (const char*) temp_total_packets);

			//printf("buffer:\n%s\n",send_buf);

			// send next packet in window
			int sent_count = sendto(fd, send_buf, strlen((const char*) send_buf), 0, (struct sockaddr*) &cliaddr, sizeof(cliaddr));
			if (sent_count < 0)
			    perror("error sending file\n");

			final_timestamp = time(NULL);
		    }
		}
	    }
	}
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

    if(argc == 6)
    {
	mode = atoi(argv[5]);
    }
    else
	mode = 0;
    loss_rate = atof(argv[3]);
    corruption_rate = atof(argv[4]);

    FILE *fp;
    unsigned char received_buf[BUFSIZE];
    unsigned char file_buf[BUFSIZE];
    unsigned char file_name[BUFSIZE];
    char *send_buf;
    int filesize;
    int recvlen;
    int seq_num; // sequence number

    //window size
    window_size = atoi(argv[2])/BUFSIZE;


    srand(SRAND);
    // other stuff
    for(;;)
    {
	memset(file_name,0,sizeof(file_name));
	// listen for receiver request/ACKs
	printf("\nwaiting\n");
	recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
	if(loss_check())
	{     
	    printf("LOST PACKET\n");
	    continue;
	}

	if (corruption_check())
	{
	    printf("CORRUPTED PACKET\n");
	    continue;
	}

	//printf("received %d bytes\n", recvlen);
	if(recvlen > 0)
	{
	    received_buf[recvlen] = 0;
	    printf("RECEIVED FILE REQUEST: %s\n",received_buf);
	}


	// send message based off request
	if(received_buf[0] != 'A' && received_buf[1] != 'C' && received_buf[2] != 'K')
	{
	    fp = fopen((const char*) received_buf, "r");
	    if(fp == NULL || corruption_check())
	    {
		perror("file not found\n");
		printf("CORRUPTED PACKET\n");
		char nack_message[5] = "NACK";
		if (sendto(fd, nack_message, sizeof(nack_message), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) <0)
		    perror("error sending nack");
		continue;
	    }
	    else if(loss_check())
	    {
		printf("LOST PACKET\n");
		continue;
	    }
	    else
	    {
		strcpy((char *) file_name, (const char*) received_buf);
		char ack_message[4] = "ACK";
		if (sendto(fd, ack_message, sizeof(ack_message), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) <0)
		    perror("error sending ack");
		// waiting for receiver to ACK about the file
		while(1)
		{
		    memset(received_buf, 0, sizeof(received_buf)); 
		    recvlen = recvfrom(fd, received_buf, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &addrlen);
		    //printf("received buf:\n%s\n",received_buf);
		    if (recvlen < 0)
		    {
			printf("error receiving message\n");
			continue;
		    }
		    if(loss_check())
		    {
			printf("LOST PACKET\n");
			memset(received_buf,0,sizeof(received_buf));
			continue;
		    }
		    else if (corruption_check())
		    {
			printf("CORRUPTED PACKET\n");
			memset(received_buf,0,sizeof(received_buf));
			continue;
		    }
		    else if (strcmp((const char *)received_buf,"ACK") == 0)
		    {
			printf("RECEIVED ACK\n");
			break;
		    }
		    else// if(strcmp(received_buf,file_name) == 0)
		    {
			printf("RECEIVED FILE REQUEST: %s\n", received_buf);
			char ack_message[4] = "ACK";
			if (sendto(fd, ack_message, sizeof(ack_message), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) <0)
			    perror("error sending ack");
		    }
		}
	    }
	    fseek(fp, 0L, SEEK_END);
	    filesize = ftell(fp);
	    fseek(fp, 0L, SEEK_SET);

	    if(mode == 0)
	    {
		ack = (char *) malloc(window_size);
		memset(ack,0,sizeof(ack));

		packet_contents = (char **) malloc(window_size * sizeof(char*));
		for(int i = 0; i < window_size; i++)
		    packet_contents[i] = (char *) malloc(BUFSIZE + HEADERSIZE);

		timestamps = (time_t *) malloc(window_size * sizeof(time_t));
		final_ack = 0;
		int window_start = 0;
		//start threading
		pthread_t threads[2];
		pthread_create(threads, NULL, listenthread, (void *) fp);
		pthread_create(threads+1,NULL, talkthread, (void *) fp);
		pthread_join(threads[0],NULL);
		pthread_join(threads[1],NULL);
	    }
	    //EC
	    else if(mode == 1)
	    {
		window_size = 1;
		threshhold_reached = 0;
		ack = (char *) malloc(window_size);
		memset(ack,0,sizeof(ack));

		packet_contents = (char **) malloc(window_size * sizeof(char*));
		for(int i = 0; i < window_size; i++)
		    packet_contents[i] = (char *) malloc(BUFSIZE + HEADERSIZE);

		timestamps = (time_t *) malloc(window_size * sizeof(time_t));
		final_ack = 0;
		int window_start = 0;
		//start threading
		pthread_t threads[2];
		pthread_create(threads, NULL, sslistenthread, (void *) fp);
		pthread_create(threads+1,NULL, sstalkthread, (void *) fp);
		pthread_join(threads[0],NULL);
		pthread_join(threads[1],NULL);
	    }
	    exit(0);

	    free(ack);
	    free(packet_contents);
	    free(timestamps);

	}
	else
	{
	    printf("received ACK\n");
	}
    }

    exit(0);
}
