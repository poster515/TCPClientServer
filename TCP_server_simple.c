// WARNING - This document contains Technical Data and/or technology whose export
// or disclosure to Non-U.S. Persons, wherever located, is restricted by the
// International Traffic in Arms Regulations (ITAR) (22 C.F.R. Section 120-130)
// or the Export Administration Regulations (EAR) (15 C.F.R Section 730-774).
// This document CANNOT be exported (e.g., provided to a supplier outside of the
// United States) or disclosed to a Non-U.S. Person, wherever located, until a
// final Jurisdiction and Classification determination has been completed and
// approved by Raytheon, and any required U.S. government approvals have been
// obtained. Violations are subject to severe criminal penalties.

#include <cstdio>
#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
#define RADIANS_TO_DEGREES (180.0/M_PI)

// flag to denote the user has stopped data collection
static volatile bool STOP = false;

// Function designed for chat between client and server. 
void listen_simple(int connfd, bool send, bool recv){ 
	// establish a muffer with max AN packet length
	char buff[AN_MAXIMUM_PACKET_SIZE]; 
	int n; 
	fd_set readfds, writefds;
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 10000;
	int n_bytes = 0;

	// infinite loop for chat 
	while(STOP != true) { 
		// clear the list of file descriptors ready to read
		FD_ZERO(&readfds);
		// add tcp_socket as a file descriptor ready to read
		FD_SET(connfd, &readfds);
		// check all file descriptors and determine if they're ready to read
		select(connfd + 1, &readfds, NULL, NULL, &t);
		if(FD_ISSET(connfd, &readfds)){
			if (recv == true){
				bzero(buff, AN_MAXIMUM_PACKET_SIZE); 

				// read the message from client and copy it in buffer 
				n_bytes = read(connfd, buff, sizeof(buff)); 
			
				// print buffer which contains the client contents 
				printf("Received %d bytes from client\n", n_bytes);
				printf("From client: '%s'\n", buff); 
			}
		}
		// if msg contains "Exit" then server exit and chat ended. 
		if (strncmp("exit", buff, 4) == 0) { 
			printf("Server Exit...\n"); 
			STOP = true; 
		} 
		// clear the list of file descriptors ready to read
		FD_ZERO(&writefds);
		// add tcp_socket as a file descriptor ready to read
		FD_SET(connfd, &writefds);
		// check all file descriptors and determine if they're ready to read
		select(connfd + 1, NULL, &writefds, NULL, &t);
		if (FD_ISSET(connfd, &writefds)){
			if (send == true){
				// erase buff contents
				bzero(buff, AN_MAXIMUM_PACKET_SIZE); 
				n = 0; 

				// obtain user input and copy into buffer 
				while ((buff[n++] = getchar()) != '\n'); 

				// and send that buffer to client 
				write(connfd, buff, sizeof(buff)); 
			}
		}
		// if msg contains "Exit" then server exit and chat ended. 
		if (strncmp("exit", buff, 4) == 0) { 
			printf("Server Exit...\n"); 
			STOP = true; 
		} 
	}
	//close(connfd); 
}
void sigintHandler(int signum){
	printf("\nCaught SIGINT, exiting program...\n");
	// write flag indicating that we should stop 
	STOP = true;
	//signal(SIGINT, sigintHandler);
}

// Driver function 
int main(int argc, char *argv[]) 
{ 
	if((argc < 3) || (argc > 4)) {
		printf("Usage: TCP_server_simple [port] [-r] [-s]\n");
        printf("Use '-r' to enable receiving data.\n");
        printf("Use '-s' to enable sending data.\n");
        printf("Must use one or both of '-s' and '-r'.\n");
		exit(EXIT_FAILURE);
	}
	signal(SIGINT, sigintHandler);
    bool send = false;
    bool recv = false;
	int port = atoi(argv[1]);

	if ((strncmp(argv[2], "-r", 2)) == 0) { 
        printf("Configuring server to receive data...\n");  
        recv = true;
        if (argc == 4){
            if ((strncmp(argv[3], "-s", 2)) == 0) {
                printf("Also configuring server to send data.\n"); 
                send = true;
            } else {
				printf("Fourth argument unknown.\n");
			}
        }
    } else if ((strncmp(argv[2], "-s", 2) == 0)) {
        printf("Configuring server to send data...\n");
        send = true;
        if (argc == 4){
            if ((strncmp(argv[3], "-r", 2)) == 0) { 
                printf("Also configuring server to receive data.\n");  
                recv = true;
            } else {
				printf("Fourth argument unknown.\n");
			}
        }
    } else {
        printf("Send/receive option not recognized. Use '-r' or '-s'.\n");
        exit(EXIT_FAILURE);
    }
	int sockfd, connfd, len; 
	struct sockaddr_in servaddr, cli; 

	// socket creation and verification. 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n"); 

	// zero the servaddr bytestring
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT. Listens to any IP address on the specific PORT
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //converts the unsigned integer hostlong from host byte order to network byte order
	servaddr.sin_port = htons(port); // converts the unsigned short integer netshort from network byte order to host byte order

	// Binding newly created socket to given IP and verify connection
	if ((bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr))) != 0) { 
		printf("Socket bind failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully bound..\n"); 

	// Now server is ready to listen. The "5" below refers to
	// the maximum number of pending connections for sockfd.
	if ((listen(sockfd, 5)) != 0) { 
		printf("Listen failed with error code: %d\n", errno); 
		exit(0); 
	} 
	else
		printf("Server listening...\n"); 

	// Accept and verify the data packet from client. 
	// accept() only used with SOCK_STREAM and SOCK_SEQPACKET.
	// Extracts first connection request and returns a new
	// file descriptor referring to the newly connected socket.
	// The newly created socket is not in the listening state.
	connfd = accept(sockfd, (sockaddr*)&cli, (socklen_t*) &len); 
    
	if (connfd < 0) { 
		printf("server accept failed with error code : %d\n", errno); 
		exit(0); 
	} 
	else
		printf("server accept success!\n"); 

	// finally, listen to bytes over the socket
	listen_simple(connfd, send, recv); // should this be sockfd?

	// After chatting close the socket 
	close(sockfd); 
} 
