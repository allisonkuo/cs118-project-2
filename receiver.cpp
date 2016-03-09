#include <sys/types.h> 
#include <sys/socket.h> 
#include <stdio.h> /* for fprintf */ 
#include <string.h> /* for memcpy */ 
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <poll.h>

#define BUFSIZE 948 // MAKE THE SAME SIZE AS MAX SENDER'S PACKET SIZE
#define HEADERSIZE 52
#define TIMEOUT 2000  // IN MILLISECONDS
#define SRAND 5

double loss_rate, corruption_rate;
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

int main(int argc,char *argv[])
{
  int fd, recvlen;
  unsigned int addrlen;

  struct hostent *hp; // host information 
  struct sockaddr_in servaddr; // server address

  char *request = argv[3]; // request message

  loss_rate = atof(argv[4]);
  corruption_rate = atof(argv[5]);
  srand(SRAND);
  // set server address information
  memset((char*)&servaddr, 0, sizeof(servaddr)); 
  servaddr.sin_family = AF_INET; 
  servaddr.sin_port = htons(atoi(argv[2])); // look up the address of the server given its name

  // create socket 
  unsigned char buf[BUFSIZE + HEADERSIZE];
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

  printf("SENT FILE REQUEST: %s\n",request);
  // listen for ACK or NACK from server

  struct pollfd fd_temp;
  int res;

  fd_temp.fd = fd;
  fd_temp.events = POLLIN;

  while(1)
  { 
      res = poll(&fd_temp, 1, 3000); // 1 sec timeout
      if (res == 0)
      {
	  printf("sent\n");
	  sendto(fd, request, strlen(request), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	  printf("SENT FILE REQUEST (RETRANSMITTED): %s\n",request);
	  continue;
      }
      recvlen = recvfrom(fd, buf, BUFSIZE + HEADERSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
      // server received the file request
      //printf("buffer:\n%s\n",buf);
      if(recvlen <= 0)
      {
	  perror("error receiving file");
      }
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
      if(strcmp((const char*) buf, "ACK") == 0)
      {
	  printf("RECEIVED ACK\n");
	  char ack_message[4] = "ACK";
	  sendto(fd, ack_message, strlen(ack_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	  printf("SENT ACK\n");
	  break;
      }
      // some error in the file request, resend
      else if(strcmp((const char*) buf, "NACK") == 0)
      {
	  printf("RECEIVED NACK\n");
	  sendto(fd, request, strlen(request), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	  printf("SENT FILE REQUEST (RETRANSMITTED): %s\n", request);
      }
  }



  char **file_content; // to hold all the packets 
  int max_packets = 100;
  int received_packets_count = 0;
  int total_packets = -1;
  file_content = (char **) malloc(100 * sizeof(char*));
  int j;
  for(j = 0; j < 100; j++)
  {
    file_content[j] = (char *)malloc(BUFSIZE + HEADERSIZE);
    memset(file_content[j],0,sizeof(file_content[j]));
  }

  int first_packet = 1;
  // listening 
  for (;;)
  {
    // reset buf 
    memset(buf, 0, BUFSIZE + HEADERSIZE);  

    printf("total packets: %d\n",total_packets);
    printf("received packets: %d\n",received_packets_count);
    if (total_packets == received_packets_count)
    {
      FILE *fp;
      fp = fopen("output.txt","w+"); // output to a file 

      // received whole message
      for (int k = 0; k < received_packets_count; k++)
      {
        fprintf(fp,"%s",file_content[k]);
        //printf("%s", file_content[k]);
      }
      fclose(fp);
  //    exit(0);
    }

    int lost = 0;
    int corrupted = 0;
    // wait for packets from server
    addrlen = sizeof(servaddr);
    if(first_packet) // waiting for packet still
    {
      res = poll(&fd_temp, 1, 3000); // 1 sec timeout
      if (res == 0) //timed out
      {
	  char ack_message[4] = "ACK";
	  sendto(fd, ack_message, sizeof(ack_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	  printf("SENT ACK (RETRANSMITTED)\n");
	  continue;
      }
      else
      {
	  recvlen = recvfrom(fd, buf, BUFSIZE + HEADERSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
	  if(loss_check())
	  {
	      printf("LOST PACKET\n");
		continue;
	  }
	  if(corruption_check())
	  {
	      printf("LOST PACKET\n");
	      continue;
	  }
	  
	  if(strstr((char*) buf, "PACKET NUMBER: ") != NULL)
	  {
	      first_packet = 0;
	  }
	  else if(strcmp((const char*)buf, "ACK") == 0)
	  {
	      char ack_message[4] = "ACK";
	      printf("RECEIVED RETRANSMITTED ACK\n");
	      sendto(fd, ack_message, sizeof(ack_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	      printf("SENT ACK (RETRANSMITTED)");
	      continue;
	  }
      }
    }
  
  else
    {
	recvlen = recvfrom(fd, buf, BUFSIZE + HEADERSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);
	if(loss_check())
	{
	    lost = 1;
	}
	if(corruption_check())
	{
	    corrupted = 1;
	}
    }
    // parse header
    // sequence number
    if(strstr((char*) buf, "PACKET NUMBER: ") == NULL)
    {
      printf("ERROR: \"PACKET NUMBER\" NOT FOUND\n");
      continue;
    }
    char* header_pos = strstr((char*) buf, "PACKET NUMBER: ") + 15;
    char* seq_pos = strstr((char*) buf, "SEQUENCE NUMBER: ") + 17;
    char sequence_num[30000];
    char r_seq_num[30000];
    int i;
    int j;
    if (header_pos[0] == '-' && header_pos[1] == '1')  // last packet header: -1
    {
      i = 2;
      j = 0;
    }

    else
    {
      for(i = 0; ; i++)
      {
        if (!isdigit(header_pos[i]))
          break;
      }
      for (j = 0; ; j++)
      {
	if (!isdigit(seq_pos[j]))
	  break;
      }
    }
    memset(sequence_num,0,sizeof(sequence_num));
    memset(r_seq_num,0,sizeof(r_seq_num));

    // extract packet's sequence number
    strncpy(sequence_num, header_pos,i);
    strncpy(r_seq_num, seq_pos,j);

    if(lost == 1)
    {
	lost = 0;
	corrupted = 0;
	printf("LOST PACKET #%s\n", r_seq_num);
	continue;
    }
    if(corrupted == 1)
    {
	lost = 0;
	corrupted = 0;
	printf("CORRUPTED PACKET #%s\n", r_seq_num);
	continue;
    }
    printf("RECEIVED PACKET NUMBER: %s\n",sequence_num);
    printf("RECEIVED SEQUENCE NUMBER: %s\n",r_seq_num);
    // send ack if received packet
    char ack[30000] = "ACK: ";  // ACK
    strcat(ack, sequence_num);
    if (sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
      perror("send to failed"); 
    printf("SENT ACK %s\n", sequence_num);//r_seq_num); //TODO

    // parse out message
    int sequence = atoi(sequence_num);
    char* message_start_position = strstr((char*) buf, "\n") + 1;


    // PLAN: last packet seq number = -1
    //       last packet message = # of total packets sent
    if (sequence == -1)
    {
      for(i = 0; ; i++)
      {
        if(!isdigit(message_start_position[i]))
          break;
      }
      char total_packets_string[30000];
      strncpy(total_packets_string, message_start_position, i);
      total_packets = atoi(total_packets_string);
      //received_packets_count += 1;
      continue;
    }

    // add to message buffer

    while (sequence >= max_packets) //allocate more memory
    {
	file_content = (char **) realloc(file_content, max_packets * 2 * sizeof(char*));
	for(j = max_packets; j < max_packets * 2; j++)
	{
	    file_content[j] = (char *) malloc(BUFSIZE + HEADERSIZE);
	    memset(file_content[j],0,sizeof(file_content[j]));
	}
	max_packets = max_packets * 2;
    }
    if(strcmp(file_content[sequence],message_start_position) != 0)
    {
      strcpy(file_content[sequence], message_start_position);
      received_packets_count += 1;
    }
  }
}
