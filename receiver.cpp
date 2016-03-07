#include <sys/types.h> 
#include <sys/socket.h> 
#include <stdio.h> /* for fprintf */ 
#include <string.h> /* for memcpy */ 
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#define BUFSIZE 1024 // MAKE THE SAME SIZE AS MAX SENDER'S PACKET SIZE
#define HEADERSIZE 5000

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


  char **file_content; // to hold all the packets 
  int max_packets = 1;
  int received_packets_count = 0;
  int total_packets = -1;
  file_content = (char **) malloc(100 * sizeof(char*));
  int j;
  for(j = 0; j < 100; j++)
  {
    file_content[j] = (char *)malloc(BUFSIZE + HEADERSIZE);
  }
 
  // listening 
  for (;;)
  {
    // reset buf 
    memset(buf, 0, BUFSIZE + HEADERSIZE);  

    printf("total packets: %d\n",total_packets);
    printf("received_packets_count: %d\n",received_packets_count);
    if (total_packets == received_packets_count)
    {
      // received whole message
      for (int k = 0; k < received_packets_count + 1; k++)
        printf("%s", file_content[k]);
    }
    // wait for packets from server
    addrlen = sizeof(servaddr);
    recvlen = recvfrom(fd, buf, BUFSIZE + HEADERSIZE, 0, (struct sockaddr *)&servaddr, &addrlen);

    // parse header
    // sequence number
    char* header_pos = strstr((char*) buf, "SEQUENCE NUMBER: ") + 17;
    char sequence_num[30000];
    int i;
    if (header_pos[0] == '-' && header_pos[1] == '1')  // last packet header: -1
      i = 2;
    else
    {
      for(i = 0; ; i++)
      {
        if (!isdigit(header_pos[i]))
          break;
      }
    }
    // extract packet's sequence number
    strncpy(sequence_num, header_pos,i);
    
    printf("sequence num: %s\n",sequence_num);
    // send ack if received packet
    char ack[30000] = "ACK: ";  // ACK
    strcat(ack, sequence_num);
    if (sendto(fd, ack, strlen(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
      perror("send to failed"); 

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
      continue;
    }

    // add to message buffer
    while (sequence >= max_packets) //allocate more memory
    {
      file_content = (char **) realloc(file_content, max_packets * 2 * sizeof(char*));
      for(j = max_packets; j < max_packets * 2; j++)
      {
        file_content[j] = (char *) malloc(BUFSIZE + HEADERSIZE);
      }
      max_packets = max_packets * 2;
    }
    
    strcpy(file_content[sequence], message_start_position);
//  printf("packet %i\n%s\n",sequence, file_content[sequence]);
    received_packets_count += 1;
  }

}
