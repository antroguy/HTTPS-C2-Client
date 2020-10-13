#include "client.h"

int client::clientInit() {
	WSADATA wsaData;
	int result;
	//Initialize Winsock (Initiate use of WS2_32.dll)
	//MAKEWORD(2,2) makes a request for version 2.2 of Winsock
	if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		//Need to improve debug logging for this....
		OutputDebugStringA("Client: Failed Initializing WSAstartup: \n");
		return result;
	}
}
int client::clientConn(client* context, std::string hostname, std::string portN) {
	int result;
	//Ensure memory of struct is clear
	ZeroMemory(&context->hints, sizeof(context->hints));
	context->hints.ai_family = AF_UNSPEC; //Unspecified so we can use either IPV4/IPV6
	context->hints.ai_socktype = SOCK_STREAM; //Stream socket
	context->hints.ai_protocol = IPPROTO_TCP; //Use TCP 
	//getaddrinfo requesting the IP address/PORT for the server name. (Translation from an ANSI host name to an address)
	if ((result = getaddrinfo(hostname.c_str(), portN.c_str(), &context->hints, &context->result)) != 0) {
		OutputDebugStringA("Client: Failed to getaddrinfo for server: \n");
		return result;
	}
	context->ptr = context->result;
	//Creates a socket that is bound to the specified transport service provider (AF_INET, Stream, TCP). We
	//Will loop through the linked list of addrinfo structure until an address works. 
	for (context->ptr = context->result; context->ptr != NULL; context->ptr = context->ptr->ai_next) {
		if ((context->sock = socket(context->ptr->ai_family, context->ptr->ai_socktype, context->ptr->ai_protocol)) == INVALID_SOCKET) {
			OutputDebugStringA("Client: Failed to create socket: \n");
			return result;
		}
		//Connect to the server 
		if ((result = connect(context->sock, context->ptr->ai_addr, (int)context->ptr->ai_addrlen)) == SOCKET_ERROR) {
			closesocket(sock);
			context->sock = INVALID_SOCKET;
			OutputDebugStringA("Client: Unable to connect to server: \n");
			return result;
		}

		break;
	}
	//Iff the ptr is null there is no valid addrinfo struct
	if (context->ptr == NULL) {
		OutputDebugStringA("Client: Unable to connect to server, null ptr returned \n");
		freeaddrinfo(context->result);
		return result;
	}
	//Don't need result anymore
	freeaddrinfo(context->result);
	return 0;
}

//Sebd request
int client::sendRequest(client* context, std::string request) {
	int result;
	if ((result = send(context->sock, request.c_str(), request.length(), 0)) == SOCKET_ERROR) {
		OutputDebugStringA("Client: Unable to send: \n");
		closesocket(sock);
		return result;
	}
	return 0;
}

//Recieve response from server
int client::recvResponse(client* context){
	int result;
	//Bufer to hold data
	char* recvBuff = (char*)malloc(sizeof(char) * 4096);

	//Recieve Header
	if ((result = recv(context->sock, recvBuff, 1024, 0)) == -1) {
		OutputDebugStringA("Client: Did not recieve data from server");
		closesocket(context->sock);
		free(recvBuff);
		return result;
	}

	//ParseHeader for attributes/values. Store them in the headerMap
	if (parseHeader(recvBuff, &context->headerMap)) {
		OutputDebugStringA("Client: Unable to open temporary file for writing\n");
		closesocket(context->sock);
		free(recvBuff);
		return -1;
	}
	//Validate Header
	/*if (valHeader(&context->headerMap)) {

		return -1;
	}*/
	//Open file for writing
	FILE* file;
	fopen_s(&file, "temp.png", "wb");
	if (file == NULL) {
		OutputDebugStringA("Client: Unable to open temporary file for writing\n");
		closesocket(context->sock);
		return result;
	}
	//write to file.
	while (result > 0) {
		result = recv(context->sock, recvBuff, 4096, 0);
		fwrite(recvBuff, 1, result, file);
		memset(recvBuff, 0, sizeof(recvBuff));
	}
	//Close socket
	closesocket(context->sock);
	//File can be closed now
	fclose(file);
	
	return 0;

}

//Parse Header, store attributes and values in a header map
int client::parseHeader(char* header, std::map<std::string, std::string>* headerMap) {
	std::istringstream resp(header);	//Input stream for string buffer
	std::string output;					//Temporary string to hold each line of the header request
	int index;							//Index for string
	int countInitial = 0;				//Used to parse HTTP version and status
	//Extract characters from the stream (Delim character is \n);
	while (std::getline(resp, output) && output != "\r") {
		//If initial, grab HTTP Version and Status Code
		if (countInitial == 0) {
			index = output.find(' ', 0);
			headerMap->insert(std::make_pair("Ver", output.substr(0, index)));
			headerMap->insert(std::make_pair("Status", output.substr(index + 1, output.length() - index - 2)));
			countInitial = 1;
		}
		//Else, grab all other attributes and values
		else {
			index = output.find(':', 0);
			if (index != std::string::npos) {
				headerMap->insert(std::make_pair(output.substr(0, index), output.substr(index + 2, output.length() - index - 3)));
			}
		}
	}
	//Clear the buffer memory
	memset(header, 0, sizeof(header));
	//If header is null, return -1
	if (headerMap->empty()) {
		return -1;
	}
	return 0;

}

//Validate Header (Validate GET Status)
int client::valHeader(std::map<std::string, std::string>* headerMap) {
	
	return 0;
}