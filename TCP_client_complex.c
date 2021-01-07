
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <termios.h>
#include "utils.c"

#define RADIANS_TO_DEGREES (180.0/M_PI)


// flag to denote the user has stopped data collection
static volatile bool STOP = false;

void sigintHandler(int signum){
	printf("\nCaught SIGINT, exiting program...\n");
	// write flag indicating that we should stop 
	STOP = true;
	//signal(SIGINT, sigintHandler);
}

int main(int argc, char *argv[])
{

	if(argc != 3)
	{
		printf("Usage - program ipaddress/hostname port \nExample - packet_example.exe an-subsonus-1.local 16718\nExample - packet_example.exe 192.168.2.20 16718\n");
		exit(EXIT_FAILURE);
	}
	signal(SIGINT, sigintHandler);

	// socket attributes
	struct sockaddr_in serveraddr;
	unsigned int bytes_received = 0;
	char *hostname;
	hostname = argv[1];
	int port = atoi(argv[2]);
	struct hostent *server;

	// socket file descriptor attributes
	struct timeval t;
	fd_set readfds;
	t.tv_sec = 1; // was 0
	t.tv_usec = 0; // was 10000

	// AN packet attributes
	an_decoder_t an_decoder;
	an_packet_t *an_packet;
	an_decoder_initialise(&an_decoder);
	subsonus_track_packet_t subsonus_track_packet;

	// make new array of remote track packets
	subsonus_track_packet_t track_packets[MAX_DATA_ENTRIES];
	int data_array_index = 0;

	// serial file descriptor attributes
	//fd_set serfds;
	//struct termios tty; // config object for serial port
	//int serial_fd = open_serial(&tty);
	//if (serial_fd < 0){
	//	printf("Error opening serial port.\n");
	//}

	// Open TCP socket
	int tcp_socket;
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_socket < 0) {
		printf("Could not open TCP socket\n");
		exit(EXIT_FAILURE);
	} else {
		printf("Successfully opened TCP socket...\n");
	}

	// CONFIGURE SERVER SETTINGS FOR CLIENT TO CONNECT
	server = gethostbyname(argv[1]);
	if(server == NULL){
		printf("Could not find host %s\n", hostname);
		exit(EXIT_FAILURE);
	} else {
		printf("Obtaining hostname...\n");
	}
	
	// Set the server's address
	memset((char *) &serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy((char *) &serveraddr.sin_addr.s_addr, (char *) server->h_addr_list[0], server->h_length);
	serveraddr.sin_port = htons(port);
	// END SERVER CONFIGURE

	// Connect to the server
	if(connect(tcp_socket, (const struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0){
		printf("Could not connect to server. Error thrown: %d\n", errno);
	} else {
		printf("Connected to server...\n");
	}

	printf("Flushing receive buffer...\n");
	flush_connection(tcp_socket);

	while(STOP != true)
	{
		// clear the list of file descriptors ready to read
		FD_ZERO(&readfds);
		// add tcp_socket as a file descriptor ready to read
		FD_SET(tcp_socket, &readfds);
		// check all file descriptors and determine if they're ready to read
		select(tcp_socket + 1, &readfds, NULL, NULL, &t);
		if(FD_ISSET(tcp_socket, &readfds))
		{
			// printf("TCP socket is ready to read...\n");
			bytes_received = recv(tcp_socket, an_decoder_pointer(&an_decoder), an_decoder_size(&an_decoder), MSG_DONTWAIT);
			// printf("Received %d new bytes from connection, decoding...\n", bytes_received);
			if(bytes_received > 0)
			{
				/* increment the decode buffer length by the number of bytes received */
				an_decoder_increment(&an_decoder, bytes_received);

				/* decode all the packets in the buffer */
				while((an_packet = an_packet_decode_dynamic(&an_decoder)) != NULL)
				{
					if(an_packet->id == packet_id_subsonus_track) /* subsonus track packet */
					{
						//printf("Remote Track Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
						/* copy all the binary data into the typedef struct for the packet */
						/* this allows easy access to all the different values             */
						if(decode_subsonus_track_packet(&subsonus_track_packet, an_packet) == 0)
						{
							// if this is in fact a track packet, determine if the fields are valid
							if ((subsonus_track_packet.data_valid.b.age_valid != 1) ||
								(subsonus_track_packet.data_valid.b.range_valid != 1) ||
								(subsonus_track_packet.data_valid.b.azimuth_valid != 1) ||
								(subsonus_track_packet.data_valid.b.elevation_valid != 1) ||
								(subsonus_track_packet.data_valid.b.raw_position_valid != 1) ||
								(subsonus_track_packet.data_valid.b.corrected_position_valid != 1)) {
								printf("Invalid packet data, ignoring.\n");
							} else {
								// fields are valid, display and save
								printf("Remote Track Packet:\n");
								printf("\tLatitude = %f, Longitude = %f, Height = %f\n", subsonus_track_packet.latitude * RADIANS_TO_DEGREES, subsonus_track_packet.longitude * RADIANS_TO_DEGREES, subsonus_track_packet.height);
								printf("\tRange = %f m, Azimuth = %f deg, Elevation = %f deg\n", subsonus_track_packet.range, subsonus_track_packet.azimuth * RADIANS_TO_DEGREES, subsonus_track_packet.elevation * RADIANS_TO_DEGREES);
								printf("\tX raw = %f m, Y raw = %f m, Z raw = %f m\n", subsonus_track_packet.raw_position[0], subsonus_track_packet.raw_position[1], subsonus_track_packet.raw_position[2]);
								printf("\tX corrected = %f m, Y corrected = %f m, Z corrected = %f m\n", subsonus_track_packet.corrected_position[0], subsonus_track_packet.corrected_position[1], subsonus_track_packet.corrected_position[2]);
								printf("\tDepth: %f\n", subsonus_track_packet.depth);
								
								// now save data into array if there is space for it. 
								if (data_array_index < MAX_DATA_ENTRIES){
									copy_packet(&subsonus_track_packet, &track_packets[data_array_index++]);
								} else {
									printf("Cannot save any more data, saving existing data.\n");
									STOP = true;
								}
							}
						}
					}

					/* Ensure that you free the an_packet when your done with it or you will leak memory */
					an_packet_free(&an_packet);
				}
			}
		}
		
		//// clear the list of file descriptors ready to write
		//FD_ZERO(&serfds);
		//// add serial_fd as a file descriptor ready to write
		//FD_SET(serial_fd, &serfds);
		//// check all file descriptors and determine if they're ready to read
		//select(serial_fd + 1, NULL, &serfds, NULL, &t);
		//if(FD_ISSET(serial_fd, &serfds))
		//{ 
		//	// do some serial stuff
		//	//printf("Able to transmit serial command.\n");
		//}
		else
		{
			usleep(100000);
		}
	}
	//// close the serial port
	//if (serial_fd >= 0){
	//	close(serial_fd);
	//}

	// close the socket 
    close(tcp_socket); 
	printf("Would you like to save data [y/n]? : ");
	if (getchar() == 'y'){
		printf("Saving data and exiting program...\n");
		write_output_file(track_packets, data_array_index);
		printf("Exiting.");
	} else {
		printf("Discarding data and exiting.\n");
	}
	
	return EXIT_SUCCESS;
}