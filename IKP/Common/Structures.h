#pragma once

#include <WinSock2.h>
#include <Windows.h>

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

typedef struct ThreadArgument {
	SOCKET socket;
	int clientNumber;
} THREAD_ARGUMENT;