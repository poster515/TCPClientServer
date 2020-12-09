#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "./lib/an_packet_protocol.h"
#include "./lib/subsonus_packets.h"
#define MAX_DATA_ENTRIES 1000

struct data_entry {
	uint32_t observer_unix_time_seconds;
	float remote_raw_position[3];
    float remote_corrected_position[3];
	float remote_depth;
};

void copy_data_entry(subsonus_track_packet_t *subsonus_track_packet, struct data_entry *data_array){
	data_array->observer_unix_time_seconds = (*subsonus_track_packet).observer_unix_time_seconds;
	memcpy(data_array->remote_raw_position, (*subsonus_track_packet).raw_position, 3 * sizeof(float));
	memcpy(data_array->remote_corrected_position, (*subsonus_track_packet).corrected_position, 3 * sizeof(float));
	data_array->remote_depth = (*subsonus_track_packet).depth;
}

void read_last_entry(struct data_entry *data_array){
	printf("Last Saved Remote Track Packet:\n");
	printf("\tX raw = %f m, Y raw = %f m, Z raw = %f m\n", data_array->remote_raw_position[0], data_array->remote_raw_position[1], data_array->remote_raw_position[2]);
	printf("\tX corrected = %f m, Y corrected = %f m, Z corrected = %f m\n", data_array->remote_corrected_position[0],data_array->remote_corrected_position[1],data_array->remote_corrected_position[2]);
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

void write_output_file(struct data_entry *data_array, const int index){
	printf("There are %d data entries to save...\n", index);
	// create filename as follows: 
	// YYYY_MM_DD_hh_mm_ss from first unix time stamp in array
	if (index > 0){
		// create file name reflecting current time.
		time_t t = time(NULL);
		struct tm *ptm = localtime(&t);
		
		char file_name[24]; // 24 characters in the formatting string below plus a null
		sprintf(file_name, "%04d_%02d_%02d_%02d_%02d_%02d.txt", 
			(ptm->tm_year)+1900, (ptm->tm_mon)+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		printf("Writing to file %s\n", file_name);
		// open and write to file
		FILE *fp;
		fp = fopen(file_name, "w+");
		if (fp != NULL){
			printf("Opened file successfully, writing data...\n");
			fputs("# All values are in meters.\n", fp);
			int i = 0;
			for(i = 0; i < index; ++i){
				// each line should have the following formatting:
				// hh:mm:ss: Xraw: {} Yraw: {} Zraw: {} Xcorr: {} Ycorr: {} Zcorr: {} Depth: {}\n
				time_t temp_t = (time_t) data_array[i].observer_unix_time_seconds;
				ptm = localtime(&temp_t);
				char entry[2288]; // length of format below plus a null
				sprintf(entry, "\r%02d:%02d:%02d: Xraw: %07.3f Yraw: %07.3f Zraw: %07.3f Xcor: %07.3f Ycor: %07.3f Zcor: %07.3f Deep: %07.3f", 
				ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
				data_array[i].remote_raw_position[0],
				data_array[i].remote_raw_position[1],
				data_array[i].remote_raw_position[2],
				data_array[i].remote_corrected_position[0],
				data_array[i].remote_corrected_position[1],
				data_array[i].remote_corrected_position[2],
				data_array[i].remote_depth);
				fputs(entry, fp);
			}
		}
		
		fclose(fp);
	} else {
		printf("No data entries saved, file not written.\n");
	}
}

void read_all_entries(struct data_entry *data_array, int index){
	int i = 0;
	for(i = 0; i < index; ++i){
		read_last_entry(&data_array[i]);
	}
}