#include<stdio.h>
#include "SubscriberService.h"

#pragma warning(disable:4996)

CRITICAL_SECTION queueAccess;
CRITICAL_SECTION message_queueAccess;
bool serverStopped = false;
int clients = 0;

HANDLE pubSubSemaphore;

SOCKET acceptedSocket;
SOCKET acceptedSockets[NUMBER_OF_CLIENTS];

SUBSCRIBER subscribers[NUMBER_OF_CLIENTS];
SUBSCRIBER_QUEUE* subQueue;
MESSAGE_QUEUE* messageQueue;
DATA poppedMessage;

HANDLE SubscriberSendThreads[NUMBER_OF_CLIENTS];
DWORD SubscriberSendThreadsID[NUMBER_OF_CLIENTS];

HANDLE SubscriberRecvThreads[NUMBER_OF_CLIENTS];
DWORD SubscriberRecvThreadsID[NUMBER_OF_CLIENTS];

HANDLE PubSub2ReceiveThread;
DWORD PubSub2ReceiveThreadId;

HANDLE PubSub2WorkThread;
DWORD PubSub2WorkThreadId;

HANDLE StopServerThread;
DWORD StopServerThreadID;

DWORD WINAPI Recieve(LPVOID lpParam)
{
	int iResult = 0;
	SOCKET acceptedSocket = *(SOCKET*)lpParam;
	char* recvRes;

	while (pubsub2_running) {

		recvRes = ReceiveFunction(acceptedSocket);
		if (strcmp(recvRes, "ErrorC") && strcmp(recvRes, "ErrorR") && strcmp(recvRes, "ErrorS"))
		{
			char delimiter[] = ":";

			char* ptr = strtok(recvRes, delimiter);

			char* topic = ptr;
			ptr = strtok(NULL, delimiter);
			char* message = ptr;

			if (!strcmp(topic, "shutdown")) {
				printf("\nPublisherService disconnected.\n");
				acceptedSocket = -1;
				free(recvRes);
				break;
			}
			else {
				ptr = strtok(NULL, delimiter);
				EnterCriticalSection(&message_queueAccess);
				Forward(messageQueue, topic, message);
				LeaveCriticalSection(&message_queueAccess);
				ReleaseSemaphore(pubSubSemaphore, 1, NULL);
				free(recvRes);
			}
		}
		else if (!strcmp(recvRes, "ErrorS")) {
			free(recvRes);
			break;
		}
		else if (!strcmp(recvRes, "ErrorC"))
		{
			printf("\nConnection with client closed.\n");
			closesocket(acceptedSocket);
			free(recvRes);
			break;
		}
		else if (!strcmp(recvRes, "ErrorR"))
		{
			printf("\nrecv failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedSocket);
			free(recvRes);
			break;

		}
	}

	return 1;
}

DWORD WINAPI SubscriberSend(LPVOID lpParam)
{
	int iResult = 0;
	THREAD_ARGUMENT argumentStructure = *(THREAD_ARGUMENT*)lpParam;

	while (pubsub2_running) {
		for (int i = 0; i < numberOfSubscribedSubs; i++)
		{
			if (argumentStructure.socket == subscribers[i].socket) {
				WaitForSingleObject(subscribers[i].hSemaphore, INFINITE);
				break;
			}
		}

		if (!pubsub2_running || !subscribers[argumentStructure.clientNumber].running)
			break;

		char* message = (char*)malloc(sizeof(DATA) + 1);

		if (message == NULL)
		{
			printf("Unable to allocate memory for the message buffer.");
			exit(0);
		}
		memcpy(message, &poppedMessage.topic, (strlen(poppedMessage.topic)));
		memcpy(message + (strlen(poppedMessage.topic)), ":", 1);
		memcpy(message + (strlen(poppedMessage.topic) + 1), &poppedMessage.message, (strlen(poppedMessage.message) + 1));

		int messageSize = strlen(message) + 1;

		int sendResult = SendFunction(argumentStructure.socket, message, messageSize);

		free(message);

		if (sendResult == -1)
			break;
	}

	return 1;
}

DWORD WINAPI SubscriberReceive(LPVOID lpParam) {
	ThreadArgument argumentRecvStructure = *(ThreadArgument*)lpParam;
	ThreadArgument argumentSendStructure = argumentRecvStructure;

	bool subRunning = true;
	bool subscribed = false;
	char* recvRes;

	
	while (pubsub2_running && subRunning) {
		recvRes = ReceiveFunction(argumentSendStructure.socket);

		if (strcmp(recvRes, "ErrorC") && strcmp(recvRes, "ErrorR") && strcmp(recvRes, "ErrorS"))
		{
			if (!strcmp(recvRes, "shutdown")) {
				printf("\nSubscriber %d disconnected.\n", argumentRecvStructure.clientNumber + 1);
				subRunning = false;

				if (subscribed) {
					subscribers[argumentSendStructure.clientNumber].running = false;
					ReleaseSemaphore(subscribers[argumentSendStructure.clientNumber].hSemaphore, 1, NULL);
					SubscriberShutDown(subQueue, argumentSendStructure.socket, subscribers);
				}
				acceptedSockets[argumentRecvStructure.clientNumber] = -1;
				free(recvRes);
				break;
			}
			else {
				if (!subscribed) {
					HANDLE hSem = CreateSemaphore(0, 0, 1, NULL);

					SUBSCRIBER subscriber;
					subscriber.socket = argumentSendStructure.socket;
					subscriber.hSemaphore = hSem;
					subscriber.running = true;
					subscribers[numberOfSubscribedSubs] = subscriber;

					argumentSendStructure.clientNumber = numberOfSubscribedSubs;

					SubscriberSendThreads[numberOfSubscribedSubs] = CreateThread(NULL, 0, &SubscriberSend, &argumentSendStructure, 0, &SubscriberSendThreadsID[numberOfSubscribedSubs]);
					numberOfSubscribedSubs++;
					subscribed = true;
				}

				EnterCriticalSection(&queueAccess);
				Subscribe(subQueue, argumentSendStructure.socket, recvRes);
				LeaveCriticalSection(&queueAccess);
				printf("\nSubscriber %d subscribed to topic: %s. \n", argumentRecvStructure.clientNumber + 1, recvRes);
				free(recvRes);
			}

		}
		else if (!strcmp(recvRes, "ErrorS")) {
			free(recvRes);
			break;
		}
		else if (!strcmp(recvRes, "ErrorC"))
		{
			printf("\nConnection with client closed.\n");
			closesocket(argumentSendStructure.socket);
			free(recvRes);
			break;
		}
		else if (!strcmp(recvRes, "ErrorR"))
		{
			printf("\nrecv failed with error: %d\n", WSAGetLastError());
			closesocket(argumentSendStructure.socket);
			free(recvRes);
			break;

		}
	}

	return 1;


}

DWORD WINAPI PubSub2Work(LPVOID lpParam) {
	int iResult = 0;
	SOCKET sendSocket;
	while (pubsub2_running) {
		WaitForSingleObject(pubSubSemaphore, INFINITE);
		if (!pubsub2_running)
			break;

		EnterCriticalSection(&message_queueAccess);
		poppedMessage = DequeueMessage(messageQueue);
		LeaveCriticalSection(&message_queueAccess);

		for (int i = 0; i < subQueue->size; i++) 
		{
			if (!strcmp(subQueue->subArray[i].topic, poppedMessage.topic)) {   
				for (int j = 0; j < subQueue->subArray[i].size; j++) 
				{
					sendSocket = subQueue->subArray[i].connSubs[j];
					for (int i = 0; i < numberOfSubscribedSubs; i++)
					{
						if (subscribers[i].socket == sendSocket) {
							ReleaseSemaphore(subscribers[i].hSemaphore, 1, NULL);
							break;
						}
					}
				}
			}

		}

	}
	return 1;
}

DWORD WINAPI StopServer(LPVOID lpParam)
{
	char input;

	while (pubsub2_running) {

		printf("\nPress X to stop server.\n");
		input = _getch();

		if (input == 'x' || input == 'X') {

			int iResult = 0;

			pubsub2_running = false;

			ReleaseSemaphore(pubSubSemaphore, 1, NULL);
			for (int i = 0; i < numberOfSubscribedSubs; i++)
			{
				ReleaseSemaphore(subscribers[i].hSemaphore, 1, NULL);
			}

			for (int i = 0; i < numberOfConnectedSubs; i++) {
				if (acceptedSockets[i] != -1) {

					iResult = shutdown(acceptedSockets[i], SD_BOTH);
					if (iResult == SOCKET_ERROR)
					{
						printf("\nshutdown failed with error: %d\n", WSAGetLastError());
						closesocket(acceptedSockets[i]);
						return 1;
					}
					closesocket(acceptedSockets[i]);
				}
			}

			iResult = shutdown(acceptedSocket, SD_BOTH);
			if (iResult == SOCKET_ERROR)
			{
				printf("\nshutdown failed with error: %d\n", WSAGetLastError());
				closesocket(acceptedSocket);
				return 1;
			}
			closesocket(acceptedSocket);

			break;
		}
	}
	return 1;
}

int main() {

	subQueue = CreateSubQueue(10);
	messageQueue = CreateMessageQueue(1000);

	AddTopics(subQueue);

	InitializeCriticalSection(&queueAccess);
	InitializeCriticalSection(&message_queueAccess);

	pubSubSemaphore = CreateSemaphore(0, 0, 1, NULL);

	SOCKET listenSocket = INVALID_SOCKET;

	int iResult;

	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return 1;
	}


	struct addrinfo* resultingAddress = NULL;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &resultingAddress);
	if (iResult != 0)
	{
		printf("\ngetaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	listenSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (listenSocket == INVALID_SOCKET)
	{
		printf("\nsocket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		WSACleanup();
		return 1;
	}

	iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("\nbind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	unsigned long int nonBlockingMode = 1;
	iResult = ioctlsocket(listenSocket, FIONBIO, &nonBlockingMode);

	if (iResult == SOCKET_ERROR)
	{
		printf("\nioctlsocket failed with error: %ld\n", WSAGetLastError());
		return 1;
	}

	freeaddrinfo(resultingAddress);

	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		printf("\nlisten failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	printf("\nServer successfully started, waiting for client connection.\n");

	PubSub2WorkThread = CreateThread(NULL, 0, &PubSub2Work, NULL, 0, &PubSub2WorkThreadId);
	StopServerThread = CreateThread(NULL, 0, &StopServer, NULL, 0, &StopServerThreadID);

	while (numberOfConnectedSubs < NUMBER_OF_CLIENTS && pubsub2_running)
	{
		int selectResult = SelectFunction(listenSocket, 'r');
		if (selectResult == -1) {
			break;
		}

		acceptedSockets[numberOfConnectedSubs] = accept(listenSocket, NULL, NULL);

		if (acceptedSockets[numberOfConnectedSubs] == INVALID_SOCKET)
		{
			printf("\naccept failed with error: %d\n", WSAGetLastError());
			closesocket(listenSocket);
			WSACleanup();
			return 1;
		}

		char* client = Connect(acceptedSockets[numberOfConnectedSubs]);
		if (!strcmp(client, "pubsub1")) {
			acceptedSocket = acceptedSockets[numberOfConnectedSubs];
			PubSub2ReceiveThread = CreateThread(NULL, 0, &Recieve, &acceptedSocket, 0, &PubSub2ReceiveThreadId);
		}
		else if (!strcmp(client, "sub")) {
			SubscriberRecvThreads[numberOfConnectedSubs] = CreateThread(NULL, 0, &SubscriberReceive, &subscriberThreadArgument, 0, &SubscriberRecvThreadsID[numberOfConnectedSubs]);
			numberOfConnectedSubs++;
		}
	}

	for (int i = 0; i < numberOfConnectedSubs; i++) {

		if (SubscriberRecvThreads[i])
			WaitForSingleObject(SubscriberRecvThreads[i], INFINITE);
	}

	for (int i = 0; i < numberOfSubscribedSubs; i++) {

		if (SubscriberSendThreads[i])
			WaitForSingleObject(SubscriberSendThreads[i], INFINITE);
	}

	if (PubSub2WorkThread) {
		WaitForSingleObject(PubSub2WorkThread, INFINITE);
	}

	if (StopServerThread) {
		WaitForSingleObject(StopServerThread, INFINITE);
	}

	printf("\nServer shutting down...\n");


	DeleteCriticalSection(&queueAccess);
	DeleteCriticalSection(&message_queueAccess);

	for (int i = 0; i < numberOfConnectedSubs; i++) {
		SAFE_DELETE_HANDLE(SubscriberRecvThreads[i]);
	}

	for (int i = 0; i < numberOfSubscribedSubs; i++) {
		SAFE_DELETE_HANDLE(SubscriberSendThreads[i]);
	}

	for (int i = 0; i < numberOfSubscribedSubs; i++)
	{
		SAFE_DELETE_HANDLE(subscribers[i].hSemaphore);
	}

	SAFE_DELETE_HANDLE(PubSub2WorkThread);
	SAFE_DELETE_HANDLE(StopServerThread);

	SAFE_DELETE_HANDLE(pubSubSemaphore);
	closesocket(listenSocket);

	free(subQueue);
	free(messageQueue->dataArray);
	free(messageQueue);
	free(subQueue->subArray);

	WSACleanup();

	return 0;
}