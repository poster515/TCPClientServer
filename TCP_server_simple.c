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
#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"

#define PORT 16740 
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
#define RADIANS_TO_DEGREES (180.0/M_PI)

// Function designed for chat between client and server. 
void listen_simple(int connfd) 
{ 
	// establish a muffer with max AN packet length
	char buff[AN_MAXIMUM_PACKET_SIZE]; 
	int n; 
	// infinite loop for chat 
	for (;;) { 
		bzero(buff, AN_MAXIMUM_PACKET_SIZE); 

		// read the message from client and copy it in buffer 
		read(connfd, buff, sizeof(buff)); 
       
		// print buffer which contains the client contents 
		printf("From client: %s\t To client : ", buff); 

		// erase buff contents
		bzero(buff, AN_MAXIMUM_PACKET_SIZE); 
		n = 0; 

		// obtain user input and copy into buffer 
		while ((buff[n++] = getchar()) != '\n') 
			; 

		// and send that buffer to client 
		write(connfd, buff, sizeof(buff)); 

		// if msg contains "Exit" then server exit and chat ended. 
		if (strncmp("exit", buff, 4) == 0) { 
			printf("Server Exit...\n"); 
			break; 
		} 
	}
}

// Driver function 
int main() 
{ 
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
	servaddr.sin_port = htons(PORT); // converts the unsigned short integer netshort from network byte order to host byte order

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
	listen_simple(connfd); // should this be sockfd?

	// After chatting close the socket 
	close(sockfd); 
} 
