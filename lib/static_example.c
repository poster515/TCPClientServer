/****************************************************************/
/*                                                              */
/*          Advanced Navigation Packet Protocol Library         */
/*          C Language Subsonus SDK, Version 2.4       		*/
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../lib/an_packet_protocol.h"
#include "../lib/subsonus_packets.h"
#include "../lib/data_dump.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#define RADIANS_TO_DEGREES (180.0/M_PI)

void an_packet_transmit(an_packet_t *an_packet)
{
	an_packet_encode(an_packet);

	/* it is up to the user to define the data transmit function which should have
	 * the prototype: send_function(uint8_t *data, int length); example usage below
	 *
	 * serial_port_transmit(an_packet_pointer(an_packet), an_packet_size(an_packet));
	 */
}

/*
 * This is an example of sending a configuration packet to Spatial.
 *
 * 1. First declare the structure for the packet, in this case filter_options_packet_t.
 * 2. Set ALL the fields of the packet structure
 * 3. Encode the packet structure into an an_packet_t using the appropriate helper function
 * 4. Transmit the packet
 */
void set_subsonus_operation_mode()
{
	an_packet_t an_packet;
	subsonus_operation_mode_packet_t subsonus_operation_mode_packet;

	/* initialise the structure by setting all the fields to zero */
	memset(&subsonus_operation_mode_packet, 0, sizeof(subsonus_operation_mode_packet_t));

	subsonus_operation_mode_packet.track_mode = Operation_Subsonus_Master;
	subsonus_operation_mode_packet.vehicle_profile = Profile_Stationary;
	subsonus_operation_mode_packet.enable_pressure_depth_aiding = TRUE;
	subsonus_operation_mode_packet.pressure_depth_offset = 0.123f;
	subsonus_operation_mode_packet.force_fixed_velocity_of_sound = FALSE;
	subsonus_operation_mode_packet.fixed_velocity_of_sound = 1500.0f;

	encode_subsonus_operation_mode_packet(&an_packet, &subsonus_operation_mode_packet);
	an_packet_transmit(&an_packet);
}

int main()
{
	an_decoder_t an_decoder;
	an_packet_t an_packet;
	int data_count = 0;
	int bytes_to_copy;
	
	subsonus_system_state_packet_t subsonus_system_state_packet;
	subsonus_track_packet_t subsonus_track_packet;
	
	an_decoder_initialise(&an_decoder);
	
	/* iterate through an example data dump from spatial */
	while(data_count < sizeof(data_dump))
	{
		/* calculate the number of bytes to copy */
		bytes_to_copy = an_decoder_size(&an_decoder);
		if(bytes_to_copy > sizeof(data_dump) - data_count) bytes_to_copy = sizeof(data_dump) - data_count;
		
		/* fill the decode buffer with the data */
		memcpy(an_decoder_pointer(&an_decoder), &data_dump[data_count], bytes_to_copy * sizeof(uint8_t));
		an_decoder_increment(&an_decoder, bytes_to_copy);
		
		/* increase the iterator by the number of bytes copied */
		data_count += bytes_to_copy;
		
		/* decode all packets in the internal buffer */
		while(an_packet_decode(&an_decoder, &an_packet))
		{
			if(an_packet.id == packet_id_subsonus_system_state) /* subsonus system state packet */
			{
				/* copy all the binary data into the typedef struct for the packet */
				/* this allows easy access to all the different values             */
				if(decode_subsonus_system_state_packet(&subsonus_system_state_packet, &an_packet) == 0)
				{
					printf("System State Packet:\n");
					printf("\tLatitude = %f, Longitude = %f, Height = %f\n", subsonus_system_state_packet.latitude * RADIANS_TO_DEGREES, subsonus_system_state_packet.longitude * RADIANS_TO_DEGREES, subsonus_system_state_packet.height);
					printf("\tRoll = %f, Pitch = %f, Heading = %f\n", subsonus_system_state_packet.orientation[0] * RADIANS_TO_DEGREES, subsonus_system_state_packet.orientation[1] * RADIANS_TO_DEGREES, subsonus_system_state_packet.orientation[2] * RADIANS_TO_DEGREES);
				}
			}
			else if(an_packet.id == packet_id_subsonus_track) /* subsonus track packet*/
			{
				/* copy all the binary data into the typedef struct for the packet */
				/* this allows easy access to all the different values             */
				if(decode_subsonus_track_packet(&subsonus_track_packet, &an_packet) == 0)
				{
					printf("Remote Track Packet:\n");
					printf("\tLatitude = %f, Longitude = %f, Height = %f\n", subsonus_track_packet.latitude * RADIANS_TO_DEGREES, subsonus_track_packet.longitude * RADIANS_TO_DEGREES, subsonus_track_packet.height);
					printf("\tRange = %f, Azimuth = %f, Elevation = %f\n", subsonus_track_packet.range, subsonus_track_packet.azimuth * RADIANS_TO_DEGREES, subsonus_track_packet.elevation * RADIANS_TO_DEGREES);
				}
			}
			else
			{
				printf("Packet ID %u of Length %u\n", an_packet.id, an_packet.length);
			}
		}
	}
	
	return EXIT_SUCCESS;
}
