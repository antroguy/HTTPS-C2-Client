#pragma once
#include <winsock2.h>
#include <WS2tcpip.h>
#include <sstream>
#include <map>
#include <iostream>
#include <stdio.h>
#include <regex>
#include <stdexcept>


//Header for Windows Sockets
#pragma comment(lib, "Ws2_32.lib")
class client
{
private:
	int parseHeader(char* header, std::map<std::string, std::string>* headerMap);
	int valHeader(std::map<std::string, std::string>* headerMap);


public:
	SOCKET sock;
	struct addrinfo* result; //A pointer to a linked list of one or more addrinfo structures that contains response info about the host
	struct addrinfo* ptr;  //Used to iterate through linked list of addrinfo structures
	struct addrinfo hints; //hints will provide hints about the type of socket the caller supports
	std::map<std::string, std::string> headerMap; //Map to hold attributes and values of HTTP header

	int clientInit();
	int clientConn(client* context, std::string hostname, std::string portN);
	int sendRequest(client* context, std::string request);
	int recvResponse(client* context);
};

