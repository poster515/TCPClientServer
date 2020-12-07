/****************************************************************/
/*                                                              */
/*          Advanced Navigation Packet Protocol Library         */
/*          C Language Static Subsonus SDK, Version 2.4         */
/*              Copyright 2020, Advanced Navigation             */
/*                                                              */
/****************************************************************/
/*
 * Copyright (C) 2020 Advanced Navigation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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

#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"

#define RADIANS_TO_DEGREES (180.0/M_PI)

uint32_t parseIPV4string(char* ipAddress)
{
	unsigned int ipbytes[4];
	sscanf(ipAddress, "%uhh.%uhh.%uhh.%uhh", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
	return ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
}

int an_packet_transmit(an_packet_t *an_packet)
{
	an_packet_encode(an_packet);
	// TODO: complete the code below to actually transmit packet:
	// write(sockfd, buff, sizeof(buff)); 
	return 0; //SendBuf(an_packet_pointer(an_packet), an_packet_size(an_packet));
}

/*
 * This is an example of sending a configuration packet to the GNSS Compass.
 *
 * 1. First declare the structure for the packet, in this case filter_options_packet_t.
 * 2. Set all the fields of the packet structure
 * 3. Encode the packet structure into an an_packet_t using the appropriate helper function
 * 4. Send the packet
 * 5. Free the packet
 */

void set_network_options()
{
	an_packet_t *an_packet = an_packet_allocate(30, packet_id_network_settings);

	network_settings_packet_t network_settings_packet;

	// initialise the structure by setting all the fields to zero
	memset(&network_settings_packet, 0, sizeof(network_settings_packet_t));

	network_settings_packet.dhcp_mode_flags.b.dhcp_enabled = 1;
	network_settings_packet.dhcp_mode_flags.b.automatic_dns = 1;
	network_settings_packet.dhcp_mode_flags.b.link_mode = link_auto;
	network_settings_packet.permanent = 1;

	network_settings_packet.static_dns_server = (uint32_t) parseIPV4string((char*)"0.0.0.0");  // usually the network modem: e.g. 192.168.1.1
	network_settings_packet.static_gateway = (uint32_t) parseIPV4string((char*)"0.0.0.0");     // usually the network modem: e.g. 192.168.1.1
	network_settings_packet.static_ip_address = (uint32_t) parseIPV4string((char*)"0.0.0.0");  // e.g. 192.168.1.20
	network_settings_packet.static_netmask = (uint32_t) parseIPV4string((char*)"255.255.255.0");     // e.g. 255.255.255.0

	encode_network_settings_packet(an_packet, &network_settings_packet);
	an_packet_encode(an_packet);

	an_packet_transmit(an_packet);

	an_packet_free(&an_packet);
}

int main(int argc, char *argv[])
{

	if(argc != 3)
	{
		printf("Usage - program ipaddress/hostname port \nExample - packet_example.exe an-subsonus-1.local 16718\nExample - packet_example.exe 192.168.2.20 16718\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in serveraddr;
	unsigned int bytes_received = 0;
	char *hostname;
	hostname = argv[1];
	int port = atoi(argv[2]);

	// Open TCP socket
	int tcp_socket;

	struct hostent *server;
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(tcp_socket < 0)
	{
		printf("Could not open TCP socket\n");
		exit(EXIT_FAILURE);
	}

	// ok so it looks like this code sets a server address and
	// attemps to open and maintain a socket with that server,
	// which is what I'd expect more from a client-side code.

	// I think this is contrary to what we actually want, which
	// is to bind to a port and IP address, and just listen to 
	// that port (i.e., behave like an actual server).

	// CONFIGURE SERVER SETTINGS FOR CLIENT TO CONNECT
	// Find the address of the host
	server = gethostbyname(argv[1]);
	if(server == NULL)
	{
		printf("Could not find host %s\n", hostname);
		exit(EXIT_FAILURE);
	}
	
	// Set the server's address
	memset((char *) &serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy((char *) &serveraddr.sin_addr.s_addr, (char *) server->h_addr_list[0], server->h_length);
	serveraddr.sin_port = htons(port);
	// END SERVER CONFIGURE

	// Connect to the server
	if(connect(tcp_socket, (const struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
	{
		printf("Could not connect to server. Error thrown: %d\n", errno);
	} 

	// Flush the socket
	{
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
	
	// printf("Encode Network Settings Packet: \n");
	// set_network_options();

	struct timeval t;
	fd_set readfds;
	t.tv_sec = 0;
	t.tv_usec = 10000;

	an_decoder_t an_decoder;
	an_packet_t *an_packet;

	an_decoder_initialise(&an_decoder);

	subsonus_system_state_packet_t system_state_packet;
	subsonus_track_packet_t subsonus_track_packet;

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(tcp_socket, &readfds);
		select(tcp_socket + 1, &readfds, NULL, NULL, &t);
		printf("Listening to socket connection...");
		if(FD_ISSET(tcp_socket, &readfds))
		{

			bytes_received = recv(tcp_socket, an_decoder_pointer(&an_decoder), an_decoder_size(&an_decoder), MSG_DONTWAIT);
			printf("Received %d new bytes from connection, decoding...", bytes_received);
			if(bytes_received > 0)
			{
				/* increment the decode buffer length by the number of bytes received */
				an_decoder_increment(&an_decoder, bytes_received);

				/* decode all the packets in the buffer */
				// when would we have more than one packet ever waiting to be decoded?
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
			}
		}
		else
		{
#ifdef _WIN32
			Sleep(10);
#else
			usleep(10000);
#endif
		}
	}

	return EXIT_SUCCESS;
}
