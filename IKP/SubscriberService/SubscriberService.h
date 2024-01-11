#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>

#include "../Common/Structures.h"
#include "../Common/Queue.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27017"
#define SERVER_SLEEP_TIME 50
#define NUMBER_OF_CLIENTS 40
#define INV_SOCKET 3435973836

bool subService_running = true;
int clientsCount = 0;

THREAD_ARGUMENT subscriberThreadArgument;
int numberOfConnectedSubs = 0;
int numberOfSubscribedSubs = 0;

void AddTopics(SUBSCRIBER_QUEUE*);
int SelectFunction(SOCKET, char);
char* ReceiveFunction(SOCKET, char*);
void Forward(MESSAGE_QUEUE* messageQueue, char* topic, char* message);
char* Connect(SOCKET);

void AddTopics(SUBSCRIBER_QUEUE* queue) {

	EnqueueSub(queue, (char*)"Sport");
	EnqueueSub(queue, (char*)"Fashion");
	EnqueueSub(queue, (char*)"Politics");
	EnqueueSub(queue, (char*)"News");
	EnqueueSub(queue, (char*)"Show business");
}

char* Connect(SOCKET acceptedSocket) {
	char* recvRes;

	recvRes = ReceiveFunction(acceptedSocket);

	return (char*)"test";
	free(recvRes);
}

int SelectFunction(SOCKET listenSocket, char rw) {
	int iResult = 0;
	do {
		FD_SET set;
		timeval timeVal;

		FD_ZERO(&set);

		FD_SET(listenSocket, &set);

		timeVal.tv_sec = 0;
		timeVal.tv_usec = 0;

		if (!subService_running)
			return -1;

		if (rw == 'r') {
			iResult = select(0, &set, NULL, NULL, &timeVal);
		}
		else {
			iResult = select(0, NULL, &set, NULL, &timeVal);
		}


		if (iResult == SOCKET_ERROR)
		{
			fprintf(stderr, "\nselect failed with error: %ld\n", WSAGetLastError());
			continue;
		}

		if (iResult == 0)
		{
			Sleep(SERVER_SLEEP_TIME);
			continue;
		}
		break;

	} while (1);
}

void Forward(MESSAGE_QUEUE* messageQueue, char* topic, char* message) {

	DATA data;
	memcpy(data.message, message, strlen(message) + 1);
	memcpy(data.topic, topic, strlen(topic) + 1);

	EnqueueMessage(messageQueue, data);

	printf("\nPublisherService  sent forward a new message from the Publisher to topic %s.\nMessage: %s\n", data.topic, data.message);
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


