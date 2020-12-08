#include <cstdio>
#include <netinet/in.h> 
#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
#define SA struct sockaddr 
#define RADIANS_TO_DEGREES (180.0/M_PI)
#define MAX 255

// User enters a message to send to server. 
void func(int sockfd) 
{ 
    // requires that sockfd has already made a remote connection 
    char buff[MAX]; 
    int n = 0; // index for buff 
    int n_bytes = 0; //number of bytes read and written 
    
    // declare some file descriptor sets for reading and writing
	fd_set readfds, writefds;
    // declare timeval for handling timeout
    struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 10000;
    // number of ready connections
    int n_ready = 0;
    while(1)
	{
        // grab some data to send. 
        // will need to update this to send Inunktun data control packets.
        bzero(buff, sizeof(buff)); 
        printf("Enter the string : "); 
        n_bytes = 0;
        n = 0; 
        while ((buff[n++] = getchar()) != '\n');

        // now determine if we can write data to server.
		// clear the list of file descriptors ready to write
		FD_ZERO(&writefds);
		// add tcp_socket as a file descriptor ready to write
		FD_SET(sockfd, &writefds);
		// check all file descriptors and determine if they're ready to write
		n_ready = select(sockfd + 1, NULL, &writefds, NULL, &t);
        
        printf("Number of ready connections: %d\n", n_ready);
		if(FD_ISSET(sockfd, &writefds))
		{
            n_bytes = write(sockfd, buff, sizeof(buff)); 
            printf("Wrote %d bytes to server.\n", n_bytes);
        }
        usleep(10000); 

        // now check if we can read anything from the server.
        bzero(buff, sizeof(buff)); 
        n = 0;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		n_ready = select(sockfd + 1, &readfds, NULL, NULL, &t);
        printf("Number of ready connections: %d\n", n_ready);
		if(FD_ISSET(sockfd, &readfds))
		{
            n_bytes = read(sockfd, buff, sizeof(buff));
            printf("Received %d bytes from server.\n", n_bytes);
        }

        printf("Data from server: %s\n", buff); 
        // if ((strncmp(buff, "exit", 4)) == 0) { 
        //     printf("Client Exit...\n"); 
        //     break; 
        // }
        usleep(10000); 
    } 
} 
  
int main(int argc, char *argv[]) 
{ 
    if(argc != 3)
	{
		printf("Usage - program port \nExample - packet_example.exe 192.168.0.3 16718\n");
		exit(EXIT_FAILURE);
	}

    int sockfd; 
    struct sockaddr_in servaddr; 
    char *server_c;
	server_c = argv[1];
	int port;
    port = atoi(argv[2]);

    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed with error code: %d.\n", errno); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 

    bzero(&servaddr, sizeof(servaddr)); 

    struct hostent *server_s;
    server_s = gethostbyname(server_c);
	if(server_s == NULL)
	{
		printf("Could not find server: %s\n", server_c);
		exit(EXIT_FAILURE);
	}
  
    // assign IP and PORT of server to connect to
    memset((char *) &servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    // servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    memcpy((char *) &servaddr.sin_addr.s_addr, (char *) server_s->h_addr_list[0], server_s->h_length);
    // equivalent to the following:
    // memcpy((char *) &servaddr.sin_addr.s_addr, (char *) server_s->h_addr, server_s->h_length);
    // inet_aton(server_c, &servaddr.sin_addr.s_addr);
    servaddr.sin_port = htons(port); 
  
    // connect the client socket to server socket 
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the server failed with error code: %d.\n", errno); 
        exit(0); 
    } 
    else
        printf("connected to the server..\n"); 
  
    // function for chat 
    func(sockfd); 
  
    // close the socket 
    close(sockfd); 
}