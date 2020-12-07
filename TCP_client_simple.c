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

// 
void decode_an_packet(char * buff, int bytes_received)
{
    an_decoder_t an_decoder;
	an_packet_t *an_packet;

	an_decoder_initialise(&an_decoder);
    // copy raw data into an_decoder buffer.
    // if this code ends up working, can simply always use an_decoder. 
    memcpy(an_decoder_pointer(&an_decoder), buff, an_decoder_size(&an_decoder));

	subsonus_system_state_packet_t system_state_packet;
	subsonus_track_packet_t subsonus_track_packet;

    if(bytes_received > 0)
    {
        /* increment the decode buffer length by the number of bytes received */
        // not sure this is really necessary but ok. 
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

// User enters a message to send to server. 
void func(int sockfd) 
{ 
    char buff[AN_DECODE_MAXIMUM_FILL_SIZE]; 
    int n; 
    for (;;) { 
        // bzero(buff, sizeof(buff)); 
        // printf("Enter the string : "); 
        // n = 0; 
        // while ((buff[n++] = getchar()) != '\n') 
        //     ; 
        // write(sockfd, buff, sizeof(buff)); 
        bzero(buff, sizeof(buff)); 
        n = read(sockfd, buff, sizeof(buff)); 
        printf("Received %d bytes from server.\n", n);
        decode_an_packet((char *)&buff, n);
        printf("Data from server: %s", buff); 
        if ((strncmp(buff, "exit", 4)) == 0) { 
            printf("Client Exit...\n"); 
            break; 
        } 
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