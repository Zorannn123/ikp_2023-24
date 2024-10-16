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
#pragma warning(disable:6001)

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27017"
#define SERVER_SLEEP_TIME 50
#define NUMBER_OF_CLIENTS 40
#define INV_SOCKET 3435973836

#define SAFE_DELETE_HANDLE(h) {if(h)CloseHandle(h);}

bool pubsub2_running = true;
THREAD_ARGUMENT subscriberThreadArgument;
int numberOfConnectedSubs = 0;
int numberOfSubscribedSubs = 0;

void AddTopics(SUBSCRIBER_QUEUE*);
int SelectFunction(SOCKET, char);
char* ReceiveFunction(SOCKET);
void Forward(MESSAGE_QUEUE*, char*, char*);
char* Connect(SOCKET);
int SendFunction(SOCKET, char*, int);
void Subscribe(SUBSCRIBER_QUEUE*, SOCKET, char*);
void SubscriberShutDown(SUBSCRIBER_QUEUE*, SOCKET, SUBSCRIBER);

char* Connect(SOCKET acceptedSocket) {
	char* recvRes;

	recvRes = ReceiveFunction(acceptedSocket);

	if (strcmp(recvRes, "ErrorC") && strcmp(recvRes, "ErrorR"))
	{
		if (!strcmp(recvRes, "pubsub1")) {
			printf("PubSub1 connected.\n");
			free(recvRes);

			return (char*)"pubsub1";
		}

		if (!strcmp(recvRes, "sub")) {

			subscriberThreadArgument.socket = acceptedSocket;
			subscriberThreadArgument.clientNumber = numberOfConnectedSubs;

			printf("\nSubscriber %d connected.\n", numberOfConnectedSubs + 1);

			free(recvRes);

			return (char*)"sub";
		}

	}
	else if (!strcmp(recvRes, "ErrorC"))
	{
		printf("\nConnection with client closed.\n");
		closesocket(acceptedSocket);
	}
	else if (!strcmp(recvRes, "ErrorR"))
	{
		printf("\nrecv failed with error: %d\n", WSAGetLastError());
		closesocket(acceptedSocket);
	}
	free(recvRes);
}

void AddTopics(SUBSCRIBER_QUEUE* queue) {
	EnqueueSub(queue, (char*)"Books");
	EnqueueSub(queue, (char*)"Football");
	EnqueueSub(queue, (char*)"Basketball");
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

		if (!pubsub2_running)
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

int SendFunction(SOCKET connectSocket, char* message, int messageSize)
{
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

void Subscribe(SUBSCRIBER_QUEUE* queue, SOCKET sub, char* topic) {
	for (int i = 0; i < queue->size; i++) {
		if (!strcmp(queue->subArray[i].topic, topic)) {
			int index = queue->subArray[i].size;
			queue->subArray[i].connSubs[index] = sub;
			queue->subArray[i].size++;
		}
	}
}

void SubscriberShutDown(SUBSCRIBER_QUEUE* queue, SOCKET acceptedSocket, SUBSCRIBER subscribers[])
{

	for (int i = 0; i < queue->size; i++)
	{
		for (int j = 0; j < queue->subArray[i].size; j++) 
		{
			if (queue->subArray[i].connSubs[j] == acceptedSocket) {   
				int index = queue->subArray[i].size - 1;
				SOCKET temp = queue->subArray[i].connSubs[index];
				if (temp != INV_SOCKET) {
					queue->subArray[i].connSubs[index] = INV_SOCKET;
					queue->subArray[i].connSubs[j] = temp;
					queue->subArray[i].size--;
				}
				else {
					queue->subArray[i].connSubs[j] = INV_SOCKET;
					queue->subArray[i].size--;
				}

			}
		}

	}

	for (int i = 0; i < numberOfSubscribedSubs; i++)
	{
		if (subscribers[i].socket == acceptedSocket) {
			subscribers[i].socket = INV_SOCKET;
			SAFE_DELETE_HANDLE(subscribers[i].hSemaphore);
			subscribers[i].hSemaphore = 0;
		}
	}
}

