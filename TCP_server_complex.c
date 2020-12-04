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

// function to flush the socket. Not sure if this is needed for the 
// server side code, or is just a nice-to-have.
void flush_socket(int &tcp_socket){
	int flush_length = 0;
	while(1)
	{
		struct timeval t;
		fd_set readfds;
		t.tv_sec = 0;
		t.tv_usec = 50000;
		unsigned char buf[1024];
		FD_ZERO(&readfds);
		FD_SET(tcp_socket, &readfds);
		select(tcp_socket + 1, &readfds, NULL, NULL, &t);
		if(FD_ISSET(tcp_socket, &readfds))
		{
			flush_length = recv(tcp_socket, buf, sizeof(buf), 0);
			if(flush_length < 100)
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
}

void listen_ANPP(int &tcp_socket){
	// first, flush the socket
	flush_socket(tcp_socket);

	// set a timeout structure used for the select() function below
	struct timeval t;
	fd_set readfds;
	t.tv_sec = 0;
	t.tv_usec = 10000;

	an_decoder_t an_decoder;
	an_packet_t *an_packet;

	an_decoder_initialise(&an_decoder);

	subsonus_system_state_packet_t system_state_packet;
	subsonus_track_packet_t subsonus_track_packet;

	unsigned int bytes_received = 0;

	while(1) 
	{
		printf("Waiting for client connections...");
		FD_ZERO(&readfds);
		FD_SET(tcp_socket, &readfds);

		// can grab the number of sockets ready to read, or do nothing with that number.
		select(tcp_socket + 1, &readfds, NULL, NULL, &t);

		if(FD_ISSET(tcp_socket, &readfds))
		{
			// recv(int sockfd, void *buf, size_t len, int flags)
			bytes_received = recv(tcp_socket, an_decoder_pointer(&an_decoder), an_decoder_size(&an_decoder), MSG_DONTWAIT);

			if(bytes_received > 0)
			{
				/* increment the decode buffer length by the number of bytes received */
				an_decoder_increment(&an_decoder, bytes_received);

				/* decode all the packets in the buffer */
				while((an_packet = an_packet_decode_dynamic(&an_decoder)) != NULL)
				{
					if(an_packet->id == packet_id_subsonus_system_state) /* system state packet */
					{
						printf("Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
						/* copy all the binary data into the typedef struct for the packet */
						/* this allows easy access to all the different values             */
						if(decode_subsonus_system_state_packet(&system_state_packet, an_packet) == 0)
						{
							printf("Subsonus System State Packet:\n");
							printf("\tLatitude = %f, Longitude = %f, Height = %f\n", system_state_packet.latitude * RADIANS_TO_DEGREES, system_state_packet.longitude * RADIANS_TO_DEGREES, system_state_packet.height);
							printf("\tRoll = %f, Pitch = %f, Heading = %f\n", system_state_packet.orientation[0] * RADIANS_TO_DEGREES, system_state_packet.orientation[1] * RADIANS_TO_DEGREES, system_state_packet.orientation[2] * RADIANS_TO_DEGREES);
						}
					}
					else if(an_packet->id == packet_id_subsonus_track) /* subsonus track packet */
					{
						printf("Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
						/* copy all the binary data into the typedef struct for the packet */
						/* this allows easy access to all the different values             */
						if(decode_subsonus_track_packet(&subsonus_track_packet, an_packet) == 0)
						{
							printf("Remote Track Packet:\n");
							printf("\tLatitude = %f, Longitude = %f, Height = %f\n", subsonus_track_packet.latitude * RADIANS_TO_DEGREES, subsonus_track_packet.longitude * RADIANS_TO_DEGREES, subsonus_track_packet.height);
							printf("\tRange = %f, Azimuth = %f, Elevation = %f\n", subsonus_track_packet.range, subsonus_track_packet.azimuth * RADIANS_TO_DEGREES, subsonus_track_packet.elevation * RADIANS_TO_DEGREES);
						}
					}
					else
					{
						printf("Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
					}

					/* Ensure that you free the an_packet when your done with it or you will leak memory */
					an_packet_free(&an_packet);
				} 
			} // bytes_received > 0
		} // if FD_ISSET()
	} // while
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
	listen_ANPP(connfd); // should this be sockfd?

	// After chatting close the socket 
	close(sockfd); 
} 
