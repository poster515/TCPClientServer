#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
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
void copy_packet(subsonus_track_packet_t *subsonus_track_packet, subsonus_track_packet_t *data_array){
	memcpy(data_array, subsonus_track_packet, sizeof(subsonus_track_packet_t));
}

void read_last_entry(struct data_entry *data_array){
	printf("Last Saved Remote Track Packet:\n");
	printf("\tX raw = %f m, Y raw = %f m, Z raw = %f m\n", data_array->remote_raw_position[0], data_array->remote_raw_position[1], data_array->remote_raw_position[2]);
	printf("\tX corrected = %f m, Y corrected = %f m, Z corrected = %f m\n", data_array->remote_corrected_position[0],data_array->remote_corrected_position[1],data_array->remote_corrected_position[2]);
}

int open_serial(struct termios *tty_ptr){
	// set termios settings, and return FD if successful
	
	int serial_fd = open("/dev/ttyS0", O_RDWR | O_NDELAY);
	if (tcgetattr(serial_fd, tty_ptr) != 0){
		printf("Error: could not open serial port.\n");
		return -1;
	} else {
		// then we were able to open the serial port.
		// set some config attributes.
		tty_ptr->c_cflag &= ~PARENB; // clear and disable parity bit
		tty_ptr->c_cflag &= ~CSTOPB; // clear stop field
		tty_ptr->c_cflag &= ~CSIZE;  // clear all bits that set the data size
		tty_ptr->c_cflag |= CS8; 	 // 8 bits per byte
		tty_ptr->c_cflag &= ~CRTSCTS; // disable RTS/CTS flow control
		tty_ptr->c_cflag |= CREAD | CLOCAL; // turn on read and ignore ctrl lines
		tty_ptr->c_cflag &= ~ICANON; // ???
		tty_ptr->c_cflag &= ~ECHO; 	 // disable echo
		tty_ptr->c_cflag &= ~ECHOE;  // disable erasure
		tty_ptr->c_cflag &= ~ECHONL; // disable new-line echo
		tty_ptr->c_cflag &= ~ISIG;   // disable interpretation of INTR, QUIT, SUSP
		tty_ptr->c_cflag &= ~(IXON | IXOFF | IXANY); // disable SW flow control
		tty_ptr->c_cflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // disable special in byte handlers
		tty_ptr->c_cflag &= ~OPOST; // disable special out bytes handling
		tty_ptr->c_cflag &= ~ONLCR; // prevent conversion of newline and carriage returns
		tty_ptr->c_cc[VTIME] = 10; // wait for up to one second for connection
		tty_ptr->c_cc[VMIN] = 0;

		// TODO: determine actual baud rate
		cfsetispeed(tty_ptr, B9600);
		cfsetospeed(tty_ptr, B9600);

		// save settings and check for error
		if (tcsetattr(serial_fd, TCSANOW, tty_ptr) != 0){
			printf("Error saving serial configuration, code: %d\n", errno);
			return -1;
		} 


		return serial_fd;
	}
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

void write_output_file(subsonus_track_packet_t *data_array, const int index){
	// Takes array of subsonus track packets and saves data to CSV file.

	printf("There are %d data entries to save.\n", index);
	// create filename as follows: 
	// YYYY_MM_DD_hh_mm_ss from first unix time stamp in array
	if (index > 0){
		// create file name reflecting current time.
		time_t t = time(NULL);
		struct tm *ptm = localtime(&t);
		
		char file_name[24]; // 24 characters in the formatting string below plus a null
		sprintf(file_name, "%04d_%02d_%02d_%02d_%02d_%02d.csv", 
			(ptm->tm_year)+1900, (ptm->tm_mon)+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		printf("Writing to file '%s'\n", file_name);
		// open and write to file
		FILE *fp;
		fp = fopen(file_name, "w+");
		if (fp != NULL){
			printf("Opened file successfully, writing data...\n");
			fputs("timestamp,", fp);
			fputs("device_address,", fp);
			//fputs("tracking_status,", fp);
			//fputs("observer_system_status,", fp);
			//fputs("observer_filter_status,", fp);
			fputs("observer_time_valid,", fp);
			fputs("observer_position_valid,", fp);
			fputs("observer_velocity_valid,", fp);
			fputs("observer_orientation_valid,", fp);
			fputs("observer_position_std_dev_valid,", fp);
			fputs("observer_orientation_std_dev_valid,", fp);
			fputs("observer_depth_valid,", fp);
			fputs("age_valid,", fp);
			fputs("range_valid,", fp);
			fputs("azimuth_valid,", fp);
			fputs("elevation_valid,", fp);
			fputs("raw_position_valid,", fp);
			fputs("corrected_position_valid,", fp);
			fputs("ned_position_valid,", fp);
			fputs("geodetic_position_valid,", fp);
			fputs("range_std_dev_valid,", fp);
			fputs("azimuth_std_dev_valid,", fp);
			fputs("elevation_std_dev_valid,", fp);
			fputs("position_std_dev_valid,", fp);
			fputs("depth_valid,", fp);
			fputs("signal_level_valid,", fp);
			fputs("signal_to_noise_ratio_valid,", fp);
			fputs("signal_correlation_ratio_valid,", fp);
			fputs("signal_correlation_interference_valid,", fp);
			fputs("observer_unix_time_seconds,", fp);
			fputs("observer_microseconds,", fp);
			fputs("observer_latitude,", fp);
			fputs("observer_longitude,", fp);
			fputs("observer_height,", fp);
			fputs("observer_velocity_north,", fp);
			fputs("observer_velocity_east,", fp);
			fputs("observer_velocity_down,", fp);
			fputs("observer_roll,", fp);
			fputs("observer_pitch,", fp);
			fputs("observer_heading,", fp);
			fputs("observer_latitude_std_dev,", fp);
			fputs("observer_longitude_std_dev,", fp);
			fputs("observer_height_std_dev,", fp);
			fputs("observer_roll_std_dev,", fp);
			fputs("observer_pitch_std_dev,", fp);
			fputs("observer_heading_std_dev,", fp);
			fputs("observer_depth,", fp);
			fputs("age_microseconds,", fp);
			fputs("remote_range,", fp);
			fputs("remote_azimuth,", fp);
			fputs("remote_elevation,", fp);
			fputs("remote_raw_x,", fp);
			fputs("remote_raw_y,", fp);
			fputs("remote_raw_z,", fp);
			fputs("remote_corrected_x,", fp);
			fputs("remote_corrected_y,", fp);
			fputs("remote_corrected_z,", fp);
			fputs("remote_north,", fp);
			fputs("remote_east,", fp);
			fputs("remote_down,", fp);
			fputs("remote_latitude,", fp);
			fputs("remote_longitude,", fp);
			fputs("remote_height,", fp);
			fputs("remote_range_std_dev,", fp);
			fputs("remote_azimuth_std_dev,", fp);
			fputs("remote_elevation_std_dev,", fp);
			fputs("remote_latitude_std_dev,", fp);
			fputs("remote_longitude_std_dev,", fp);
			fputs("remote_height_std_dev,", fp);
			fputs("remote_depth,", fp);
			fputs("signal_level,", fp);
			fputs("signal_to_noise_ratio,", fp);
			fputs("signal_correlation_ratio,", fp); //70
			fputs("signal_correlation_interference", fp);

			int i = 0;
			for(i = 0; i < index; ++i){
				time_t temp_t = (time_t) data_array[i].observer_unix_time_seconds;
				ptm = localtime(&temp_t);
				
				char entry[39]; // length of format below plus a null

				sprintf(entry, "\r%02d:%02d:%02d,",ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
				fputs(entry, fp);
				sprintf(entry, "%07d,", data_array[i].device_address);
				fputs(entry, fp);
				//sprintf(entry, "%07.3f,", data_array[i].tracking_status);
				//fputs(entry, fp);
				//sprintf(entry, "%07.3f,", data_array[i].observer_system_status);
				//fputs(entry, fp);
				//sprintf(entry, "%07.3f,", data_array[i].observer_filter_status);
				//fputs(entry, fp);

				// data valid flags
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_time_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_position_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_velocity_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_orientation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_position_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_orientation_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_depth_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.age_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.range_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.azimuth_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.elevation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.observer_position_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.corrected_position_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.ned_position_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.geodetic_position_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.range_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.azimuth_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.elevation_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.position_standard_deviation_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.depth_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.signal_level_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.signal_to_noise_ratio_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.signal_correlation_ratio_valid);
				fputs(entry, fp);
				sprintf(entry, "%1d,", data_array[i].data_valid.b.signal_correlation_interference_valid);
				fputs(entry, fp);
				// end valid flags
				sprintf(entry, "%09d,", data_array[i].observer_unix_time_seconds);
				fputs(entry, fp);
				sprintf(entry, "%09d,", data_array[i].observer_microseconds);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_latitude);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_longitude);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_height);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_velocity[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_velocity[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_velocity[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,,", data_array[i].observer_position_standard_deviation[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_position_standard_deviation[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_position_standard_deviation[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation_standard_deviation[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation_standard_deviation[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_orientation_standard_deviation[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].observer_depth);
				fputs(entry, fp);
				sprintf(entry, "%09d,", data_array[i].age_microseconds);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].range);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].azimuth);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].elevation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].raw_position[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].raw_position[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].raw_position[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].corrected_position[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].corrected_position[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].corrected_position[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].ned_position[0]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].ned_position[1]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].ned_position[2]);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].latitude);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].longitude);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].height);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].range_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].azimuth_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].elevation_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].latitude_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].longitude_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].height_standard_deviation);
				fputs(entry, fp);
				sprintf(entry, "%07.3f,", data_array[i].depth);
				fputs(entry, fp);
				sprintf(entry, "%04d,", data_array[i].signal_level);
				fputs(entry, fp);
				sprintf(entry, "%04d,", data_array[i].signal_to_noise_ratio);
				fputs(entry, fp);
				sprintf(entry, "%04d,", data_array[i].signal_correlation_ratio);
				fputs(entry, fp);
				sprintf(entry, "%04d\n", data_array[i].signal_correlation_interference);
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


uint32_t parseIPV4string(char* ipAddress)
{
	unsigned int ipbytes[4];
	sscanf(ipAddress, "%uhh.%uhh.%uhh.%uhh", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
	return ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
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
void set_network_options(int * sockfd)
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

	an_packet_transmit(an_packet, sockfd);

	an_packet_free(&an_packet);
}