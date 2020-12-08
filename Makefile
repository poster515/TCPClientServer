all: server_simple server_complex client_modem client_complex 

server_simple:
	g++ -Wall  TCP_server_simple.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_server_simple

server_complex: 
	g++ -Wall  TCP_server_complex.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_server_complex

client_modem: 
	g++ -Wall  TCP_client_modem.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_client_modem 

client_complex:
	g++ -Wall  TCP_client_complex.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_client_complex

