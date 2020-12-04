all: server_simple server_complex client_simple client_complex

server_simple:
	g++ -Wall  TCP_server_simple.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_server_simple

server_complex: 
	g++ -Wall  TCP_server_complex.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_server_complex

client_simple: 
	g++ -Wall  TCP_client_simple.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_client_simple

client_complex:
	g++ -Wall  TCP_client_complex.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_client_complex