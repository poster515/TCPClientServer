

all: client_modem client_complex 

client_modem: 
	g++ -Wall TCP_client_modem.c -o TCP_client_modem.exe -lws2_32

client_complex:
	g++ -Wall  TCP_client_complex.c ./lib/an_packet_protocol.c ./lib/subsonus_packets.c -o TCP_client_complex

clean:
	rm TCP_client_complex TCP_client_modem