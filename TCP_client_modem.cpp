// #include <cstdio>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <cstring>
#include <iostream>

#ifdef _WIN64
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

// #pragma comment(lib, "Ws2_32.lib")

#else
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
#define SA struct sockaddr 
#define RADIANS_TO_DEGREES (180.0/M_PI)
#define MAX 255

static volatile bool STOP = false;

// TO COMPILE:
// g++ -Wall .\TCP_client_modem.cpp -o TCP_client_modem.exe -O1 -lws2_32

// User enters a message to send to server. 
void func(int &sockfd, bool send, bool recv) 
{ 
    // requires that sockfd has already made a remote connection 
    char buff[MAX]; 
    int n = 0; // index for buff 
    int n_bytes = 0; //number of bytes read and written 
    
    // declare some file descriptor sets for reading and writing
	fd_set readfds, writefds;
    // declare timeval for handling timeout
    struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 10000;

    while(STOP == false)
	{
        if (send) {
            // grab some data to send.
            memset(buff, 0, sizeof(buff)); 
            n = 0;
            n_bytes = 0;
            printf("Enter your command: "); 
            while ((buff[n++] = getchar()) != '\n');
            printf("Message to send: %s\n", &buff[0]);
            FD_ZERO(&writefds);
            FD_SET(sockfd, &writefds);
            select(sockfd + 1, NULL, &writefds, NULL, &t);
            if(FD_ISSET(sockfd, &writefds)){
                n_bytes = write(sockfd, buff, n); // only write n bytes over pipe
                if (n_bytes > 0){
                    printf("Wrote %d bytes to server.\n", n_bytes);
                } else {
                    printf("ERROR: %s\n", std::strerror(errno));
                }
            }
        } 
        
        if (recv) {
            // now check if we can read anything from the server.
            memset(buff, 0, sizeof(buff)); 
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            select(sockfd + 1, &readfds, NULL, NULL, &t);
            if(FD_ISSET(sockfd, &readfds))
            {
                n_bytes = read(sockfd, buff, sizeof(buff));
                printf("Received %d bytes from server.\n", n_bytes);
                printf("Data from server: %s\n", buff); 
            }
        } 

        if ((strncmp(buff, "exit", 4)) == 0) { 
            break; 
        }
        usleep(10000); 
    } 
    printf("Client terminating...\n");
} 

void sigintHandler(int signum){
	printf("\nCaught SIGINT, exiting program...\n");
	STOP = true;
}
  
int main(int argc, char *argv[]) 
{ 
    if((argc < 4) || (argc > 5)) {
		printf("Usage: TCP_client_modem [IP_addr] [port] [-r] [-s]\n");
        printf("Use '-r' to enable receiving data.\n");
        printf("Use '-s' to enable sending data.\n");
        printf("Must use one or both of '-s' and '-r'.\n");
		exit(EXIT_FAILURE);
	}
    signal(SIGINT, sigintHandler);
    bool send = false;
    bool recv = false;
    int sockfd; 
    struct sockaddr_in servaddr; 
    // PARSE ARGUMENTS
    char *server_c;
	server_c = argv[1];
	int port;
    port = atoi(argv[2]);
    if ((strncmp(argv[3], "-r", 2)) == 0) { 
        printf("Configuring client to receive data...\n");  
        recv = true;
        if (argc == 5){
            if ((strncmp(argv[4], "-s", 2)) == 0) {
                printf("Also configuring client to send data.\n"); 
                send = true;
            } else {
                printf("Uknown fifth option. Please use '-s' instead of %s", argv[4]);
            }
        }
    } else if ((strncmp(argv[3], "-s", 2) == 0)) {
        printf("Configuring client to send data...\n");
        send = true;
        if (argc == 5){
            if ((strncmp(argv[4], "-r", 2)) == 0) { 
                printf("Also configuring client to receive data.\n");  
                recv = true;
            } else {
                printf("Uknown fifth option. Please use '-r' instead of %s", argv[4]);
            }
        }
    } else {
        printf("Send/receive option not recognized. Use '-r' or '-s'.\n");
        exit(EXIT_FAILURE);
    }

    // need to do this for Windows OS. 
    #ifdef _WIN64
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
	#endif

    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed with error code: %d.\n", errno); 
        exit(0); 
    } 
    else
        printf("Socket successfully created...\n"); 

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
    memcpy((char *) &servaddr.sin_addr.s_addr, (char *) server_s->h_addr_list[0], server_s->h_length);
    servaddr.sin_port = htons(port); 
  
    // connect the client socket to server socket 
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
        printf("Connection with the server failed with error code: %d.\n", errno); 
        exit(0); 
    } 
    else
        printf("Connected to the server...\n"); 
  
    // function for chat 
    func(sockfd, send, recv); 
  
    // close the socket 
    printf("Closing socket...\n"); 
    close(sockfd); 

    #ifdef _WIN64
    printf("Performing Windows cleanup...\n"); 
	WSACleanup();
	#endif

    return EXIT_SUCCESS;
}