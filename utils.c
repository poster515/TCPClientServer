#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stddef.h>
#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"
#define MAX_DATA_ENTRIES 1000

struct data_entry{
	uint32_t observer_unix_time_seconds;
	float remote_raw_position[3];
    float remote_corrected_position[3];
	float remote_depth;
};

void copy_data_entry(subsonus_track_packet_t *subsonus_track_packet, struct data_entry *data_array[], int data_array_index){
	(*data_array)[data_array_index].observer_unix_time_seconds = (*subsonus_track_packet).observer_unix_time_seconds;
	memcpy((*data_array)[data_array_index].remote_raw_position, (*subsonus_track_packet).raw_position, 3 * sizeof(float));
	memcpy((*data_array)[data_array_index].remote_corrected_position, (*subsonus_track_packet).corrected_position, 3 * sizeof(float));
	(*data_array)[data_array_index].remote_depth = (*subsonus_track_packet).depth;
}

int an_packet_transmit(an_packet_t *an_packet, int * sockfd)
{
	// set the header fields and calculate CRCs
	an_packet_encode(an_packet);
	int n_bytes = 0; // number of bytes written during write operation
	// declare some file descriptor sets for reading and writing
	fd_set writefds;
	// clear the list of file descriptors, and add sockfd as being ready to write
	FD_ZERO(&writefds);
	FD_SET(*(sockfd), &writefds);
	// check all file descriptors and determine if they're ready to write.
	// NOTE: the following line is a BLOCKING function call. 
	select(*(sockfd) + 1, NULL, &writefds, NULL, NULL);
	// TODO: test this code. Not sure if we can just send data like this. 
	if(FD_ISSET(*(sockfd), &writefds))
	{
		n_bytes = write(*(sockfd), &(an_packet->header), AN_PACKET_HEADER_SIZE*sizeof(uint8_t)); 
		printf("Wrote %d bytes of header data to server.\n", n_bytes);
		n_bytes = write(*(sockfd), &(an_packet->data), (an_packet->length)*sizeof(uint8_t)); 
		printf("Wrote %d bytes of packet data to server.\n", n_bytes);
	}
	return 0; 
}

void flush_connection(int tcp_socket)
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

void write_output_file(struct data_entry *data_array[]){
	FILE *fp;
	// TODO: create filename as follows: 
	// YYYY_MM_DD_hh_mm_ss from first unix time stamp in array
	const char * file_name = "";
	fp = fopen(file_name, "w+");
	if (fp != NULL){
		int i = 0;
		for(i = 0; i < MAX_DATA_ENTRIES; ++i){
			// each line should have the following formatting:

			// hh:mm:ss: Xraw: {} Yraw: {} Zraw: {} Xcorr: {} Ycorr: {} Zcorr: {} Depth: {}\n

			char entry  = "";
			fputs("", fp);
		}
	}
	
	fclose(fp);
}