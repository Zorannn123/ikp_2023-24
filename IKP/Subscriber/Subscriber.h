#pragma once
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include "../Common/Queue.h"
#include "../Common/Structures.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 27017
#define SERVER_SLEEP_TIME 50
#define NUMBER_OF_CLIENTS 40
#define INV_SOCKET 3435973836

#define SAFE_DELETE_HANDLE(h) {if(h)CloseHandle(h);}

bool sub_running = true;

void PrintMenu();
void ProcessInput(char, char*);
int Connect(SOCKET);
int SendFunction(SOCKET, char*, int);
int SelectFunction(SOCKET, char);
char* ReceiveFunction(SOCKET);
bool AlreadySubscribed(char, int[], int);

void PrintMenu() {
	printf("\nChoose a topic: \n");
	printf("\t1.Books\n");
	printf("\t2.Football\n");
	printf("\t3.Basketball\n");
	printf("Press X if you want to close connection.\n");
}

void ProcessInput(char input, char* message) {
	if (input == '1') {
		strcpy(message, "Books");
		printf("You subscribed to topic Books.\n");
	}
	else if (input == '2') {
		strcpy(message, "Football");
		printf("You subscribed to topic Football.\n");
	}
	else if (input == '3') {
		strcpy(message, "Basketball");
		printf("You subscribed to topic Basketball.\n");
	}
}

int Connect(SOCKET connectSocket) {
	char* connectMessage = (char*)malloc(4 * sizeof(char));

	if (connectMessage == NULL)
	{
		printf("Unable to allocate memory for the connect message buffer");
		exit(0);
	}
	strcpy(connectMessage, "sub");

	int messageSize = strlen(connectMessage) + 1;

	int retVal = SendFunction(connectSocket, connectMessage, messageSize);
	free(connectMessage);

	return retVal;
}

int SendFunction(SOCKET connectSocket, char* message, int messageSize) {

	int selectResult = SelectFunction(connectSocket, 'w');
	if (selectResult == -1) {
		return -1;
	}
	int iResult = send(connectSocket, message, messageSize, 0);

	if (iResult == SOCKET_ERROR)
	{
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(connectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

int SelectFunction(SOCKET listenSocket, char rw) {

	int iResult = 0;
	do {
		FD_SET set;
		struct timeval timeVal;

		FD_ZERO(&set);

		FD_SET(listenSocket, &set);

		timeVal.tv_sec = 0;
		timeVal.tv_usec = 0;

		if (!sub_running) {
			return -1;
		}

		if (rw == 'r') {
			iResult = select(0, &set, NULL, NULL, &timeVal);
		}
		else {
			iResult = select(0, NULL, &set, NULL, &timeVal);
		}


		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "select failed with error: %ld\n", WSAGetLastError());
			continue;
		}

		if (iResult == 0)
		{
			Sleep(SERVER_SLEEP_TIME);
			continue;
		}
		break;

	} while (1);

	return 1;
}

char* ReceiveFunction(SOCKET acceptedSocket) {

	int iResult;
	char* myBuffer = (char*)(malloc(DEFAULT_BUFLEN));

	if (myBuffer == NULL)
	{
		printf("Unable to allocate memory for the BUFFER");
		exit(0);
	}

	int selectResult = SelectFunction(acceptedSocket, 'r');
	if (selectResult == -1) {
		memcpy(myBuffer, "ErrorS", 7);
		return myBuffer;
	}
	iResult = recv(acceptedSocket, myBuffer, 256, 0);

	if (iResult > 0)
	{
		myBuffer[iResult] = '\0';
	}
	else if (iResult == 0)
	{
		memcpy(myBuffer, "ErrorC", 7);
	}
	else
	{
		memcpy(myBuffer, "ErrorR", 7);
	}
	return myBuffer;
}

bool AlreadySubscribed(char input, int topics[], int topicCount) {
	for (int i = 0; i < topicCount; i++) {
		if (topics[i] == input - '0') {
			return true;
		}
	}
	return false;
}