#include <cstring>    // sizeof()
#include <iostream>
#include <string>   
#include <unistd.h>    // close()
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#ifdef _WIN64
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
// headers for socket(), getaddrinfo() and friends
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

// TO COMPILE:
// C:/Qt/Tools/mingw1120_64/bin/g++.exe -Wall .\TCP_server.cpp -o TCP_server.exe -O1 -lws2_32

volatile std::atomic_bool STOP (false);

void sigintHandler(int signum){
	printf("\nCaught SIGINT, exiting program...\n");
	STOP = true;
}

int main(int argc, char *argv[])
{
    // Let's check if port number is supplied or not..
    if (argc != 2) {
        std::cerr << "Run program as 'TCP_server.exe <port>'\n";
        return -1;
    }
    // connect our SIGINT handler
    signal(SIGINT, sigintHandler);
    
    auto &portNum = argv[1];
    const unsigned int backLog = 8;  // number of connections allowed on the incoming queue

    // need to do this for Windows OS. 
    #ifdef _WIN64
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
	#endif

    addrinfo hints, *res, *p;    // we need 2 pointers, res to hold and p to iterate over
    memset(&hints, 0, sizeof(hints));

    // for more explanation, man socket
    hints.ai_family   = AF_UNSPEC;    // don't specify which IP version to use yet
    hints.ai_socktype = SOCK_STREAM;  // SOCK_STREAM refers to TCP, SOCK_DGRAM will be?
    hints.ai_flags    = AI_PASSIVE;

    // man getaddrinfo
    int gAddRes = getaddrinfo(NULL, portNum, &hints, &res);
    if (gAddRes != 0) {
        std::cerr << gai_strerror(gAddRes) << "\n";
        return -2;
    }

    std::cout << "Detecting addresses..." << std::endl;

    signed int numOfAddr = 0;
    char ipStr[INET6_ADDRSTRLEN];    // ipv6 length makes sure both ipv4/6 addresses can be stored in this variable

    // Now since getaddrinfo() has given us a list of addresses
    // we're going to iterate over them and ask user to choose one
    // address for program to bind to
    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;
        std::string ipVer;

        // if address is ipv4 address
        if (p->ai_family == AF_INET) {
            ipVer             = "IPv4";
            sockaddr_in *ipv4 = reinterpret_cast<sockaddr_in *>(p->ai_addr);
            addr              = &(ipv4->sin_addr);
            ++numOfAddr;
        }

        // if address is ipv6 address
        else {
            ipVer              = "IPv6";
            sockaddr_in6 *ipv6 = reinterpret_cast<sockaddr_in6 *>(p->ai_addr);
            addr               = &(ipv6->sin6_addr);
            ++numOfAddr;
        }

        // convert IPv4 and IPv6 addresses from binary to text form
        inet_ntop(p->ai_family, addr, ipStr, sizeof(ipStr));
        std::cout << "(" << numOfAddr << ") " << ipVer << " : " << ipStr << std::endl;
    }

    // if no addresses found :(
    if (!numOfAddr) {
        std::cerr << "Found no host address to use\n";
        return -3;
    }

    // ask user to choose an address
    std::cout << "Enter the number of host address to bind with: ";
    signed int choice = 0;
    bool madeChoice     = false;
    do {
        std::cin >> choice;
        if (choice > (numOfAddr + 1) || choice < 1) {
            madeChoice = false;
            std::cout << "Wrong choice, try again!" << std::endl;
        } else
            madeChoice = true;
    } while (!madeChoice);

    // now actually select the addrInfo the user selected
    p = res;    // assume the first addr was chosen 
    sockaddr_in6 *ipv6 = p->ai_family != AF_INET ? reinterpret_cast<sockaddr_in6 *>(p->ai_addr) : nullptr;
    sockaddr_in *ipv4 = p->ai_family == AF_INET ? reinterpret_cast<sockaddr_in *>(p->ai_addr) : nullptr;
    while (--choice > 0){
        p = res->ai_next;
        ipv6 = p->ai_family != AF_INET ? reinterpret_cast<sockaddr_in6 *>(p->ai_addr) : nullptr;
        ipv4 = p->ai_family == AF_INET ? reinterpret_cast<sockaddr_in *>(p->ai_addr) : nullptr;
    }    

    // convert IPv4 and IPv6 addresses from binary to text form
    void *addr = ipv4 != nullptr ? (void*)&(ipv4->sin_addr) : (void*)&(ipv6->sin6_addr);
    inet_ntop(p->ai_family, addr, ipStr, sizeof(ipStr));
    std::string ipVer = p->ai_family == AF_INET ? "IPv4" : "IPv6";
    std::cout << "Listening on " << ipVer << " address: " << ipStr << std::endl;

    // let's create a new socket, socketFD is returned as descriptor
    // man socket for more information
    // these calls usually return -1 as result of some error
    int sockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockFD == -1) {
        std::cerr << "Error while creating socket\n";
        freeaddrinfo(res);
        return -4;
    }

    // Let's bind address to our socket we've just created
    int bindR = bind(sockFD, p->ai_addr, p->ai_addrlen);
    if (bindR == -1) {
        std::cerr << "Error while binding socket\n";
        
        // if some error occurs, make sure to close socket and free resources
        close(sockFD);
        freeaddrinfo(res);
        return -5;
    }

    // finally start listening for connections on our socket
    int listenR = listen(sockFD, backLog);
    if (listenR == -1) {
        std::cerr << "Error while Listening on socket\n";

        // if some error occurs, make sure to close socket and free resources
        close(sockFD);
        freeaddrinfo(res);
        return -6;
    }
    
    // structure large enough to hold client's address
    sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    const std::string response = "Hello World";

    // a fresh infinite loop to communicate with incoming connections
    // this will take client connections one at a time
    // in further examples, we're going to use fork() call for each client connection

    #define BUFFER_LEN 1024
    typedef char buffer[BUFFER_LEN];
    buffer socket_buffer;
    int buffer_size(0);
    while (!STOP) {

        // Accept call will give us a new socket descriptor.
        // The newly created socket is not in the listening state.
        // The original socket sockfd is unaffected by this call.
        int newFD = accept(sockFD, (sockaddr *) &client_addr, &client_addr_size);
        if (newFD == -1) {
            std::cerr << "Error while Accepting on socket\n";
            continue;
        }
        // with a fresh connection, listen to the incoming data until 'STOP' is received
        #define SOCKET_CLOSE_PHRASE "STOP"
        memset(&socket_buffer[0], 0, sizeof(socket_buffer));
        while (!STOP){
            // now that client and server are connected, try to receive some data
            int bytes_received = recv(newFD, &socket_buffer[buffer_size], BUFFER_LEN, 0);
            if (bytes_received > 0){
                // can also use find() if we don't receive entire END phrase at once.
                if (std::string(&socket_buffer[buffer_size], bytes_received).compare(SOCKET_CLOSE_PHRASE) == 0){
                    std::cout << "Received stop condition phrase, closing socket.\n";
                    break; 
                } else {
                    std::cout << "Received " << bytes_received << " new bytes: " << &socket_buffer[buffer_size] << "\n";
                    buffer_size += bytes_received;
                }
            } else if (bytes_received == 0){
                std::cout << "No new data to receive.\n";
            } else {
                std::cout << "Error on socket read: " << std::strerror(errno) << ", closing socket.\n";
                break;
            }
        }
        std::cout << "Closing current connection...\n";
        closesocket(newFD);
    }
    std::cout << "Closing server socket...\n";
    close(sockFD);
    freeaddrinfo(res);

    #ifdef _WIN64
    printf("Performing Windows cleanup...\n"); 
	WSACleanup();
	#endif

    return 0;
}