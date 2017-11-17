
#define _CRT_SECURE_NO_WARNINGS


#include "stdafx.h"

#include <stdio.h>  
#include <stdlib.h>  
#include <process.h>  

#include <winsock2.h>  

#include <windows.h>  

#include <iostream>
#include <list>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

#define BUF_SIZE 1000  
#define READ    3  
#define WRITE   5  

enum class BufferState { Read, Write };

class SocketInfo {
public:
	SOCKET socket;
	SOCKADDR_IN socketAddr;
};

class BufferInfo {
public:
	OVERLAPPED  overlapped;
	WSABUF wsaBuf;
	string name;
	BufferState buffState;
};

typedef struct    // socket info  
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct    // buffer info  
{
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	int rwMode;    // READ or WRITE 
} PER_IO_DATA, *LPPER_IO_DATA;

 unsigned int __stdcall  EchoThread(LPVOID CompletionPortIO);
void ErrorHandling(char *message);
SOCKET ALLCLIENT[100];
int clientcount = 0;
HANDLE hMutex;//

class Server {
public:
	int numOfClient;
	HANDLE mutex;
	WSADATA wsaData;
	HANDLE iocpObj;
	SYSTEM_INFO sysInfo;

	SocketInfo sockInfo;

	DWORD recvBytes = 0;
	DWORD flags = 0;

list <SOCKET> socketList;
	

	void serverInit() {
		mutex = CreateMutex(NULL, FALSE, NULL);

		WSAStartup(MAKEWORD(2, 2), &wsaData); // 1.  Function Must called once until use winSock
			
		iocpObj = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);// 2. Create IOCP OBJ

		GetSystemInfo(&sysInfo);//  get system Information

		for (int i = 0; i < sysInfo.dwNumberOfProcessors; i++)  //  dwNumberOfProcessors---The number of physical processors
			_beginthreadex(NULL, 0, EchoThread, (LPVOID)iocpObj, 0, NULL);
	}

};


int main(int argc, char* argv[])
{

	Server server;
	server.serverInit();

	//BufferInfo buffInfo;


	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;
	//------------

	server.sockInfo.socketAddr;
	server.sockInfo.socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);//4. Create socket  
	memset(&server.sockInfo.socketAddr, 0, sizeof(server.sockInfo.socketAddr));
	server.sockInfo.socketAddr.sin_family = AF_INET;
	server.sockInfo.socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sockInfo.socketAddr.sin_port = htons(1234);

	bind(server.sockInfo.socket, (SOCKADDR*)&server.sockInfo.socketAddr, sizeof(server.sockInfo.socketAddr));
	listen(server.sockInfo.socket, 5);      //  5.binding  & listening
								
	while (1)
	{
		SOCKET hClntSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);

		hClntSock = accept(server.sockInfo.socket, (SOCKADDR*)&clntAdr, &addrLen);//If new client coming, go on

		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSock = hClntSock;

		WaitForSingleObject(hMutex, INFINITE);// Waiting for another thread to release the mutex

		ALLCLIENT[clientcount++] = hClntSock;//  ALLCLIENT[] Lock

		ReleaseMutex(hMutex);//

		memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

		CreateIoCompletionPort((HANDLE)hClntSock, server.iocpObj, (DWORD)handleInfo, 0);// binding
																				    
		ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));  //Reset Buffer ,Wait for Message
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->rwMode = READ;

		WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf),
			1, &server.recvBytes, &server.flags, &(ioInfo->overlapped), NULL);
	}



	CloseHandle(hMutex);//  Destroy Mutex
	return 0;
}

 unsigned int __stdcall EchoThread(LPVOID pComPort)
{
	HANDLE hComPort = (HANDLE)pComPort;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;
	DWORD flags = 0;

	while (1)
	{
		GetQueuedCompletionStatus(hComPort, &bytesTrans,
			(LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);// get socket from CP
		sock = handleInfo->hClntSock;

		if (ioInfo->rwMode == READ)
		{
			puts("message received!");
			if (bytesTrans == 0)    // The connection is over
			{
				WaitForSingleObject(hMutex, INFINITE);

				closesocket(sock);
				int i = 0;
				while (ALLCLIENT[i] != sock) { 
					i++; }
				ALLCLIENT[i] = 0;
				clientcount--;

				ReleaseMutex(hMutex);

				free(handleInfo); free(ioInfo);
				continue;
			}
			int i = 0;

			for (; i < clientcount; i++)
			{
				if (ALLCLIENT[i] != 0)//socket Exist  
				{
					if (ALLCLIENT[i] != sock)
					{
						LPPER_IO_DATA newioInfo;
						newioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
						memset(&(newioInfo->overlapped), 0, sizeof(OVERLAPPED));
						strncpy_s(newioInfo->buffer,ioInfo->buffer,_TRUNCATE);
						newioInfo->wsaBuf.buf = newioInfo->buffer;    //user who sent the message 
						newioInfo->wsaBuf.len = bytesTrans;
						newioInfo->rwMode = WRITE;

						WSASend(ALLCLIENT[i], &(newioInfo->wsaBuf),
							1, NULL, 0, &(newioInfo->overlapped), NULL);
					}
					else    
					{
						memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
						ioInfo->wsaBuf.len = bytesTrans;
						ioInfo->rwMode = WRITE;
						WSASend(ALLCLIENT[i], &(ioInfo->wsaBuf),
							1, NULL, 0, &(ioInfo->overlapped), NULL);
					}
				}
			}
			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA)); //Reset Buffer ,Wait for Message
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			WSARecv(sock, &(ioInfo->wsaBuf),
				1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else
		{
			puts("message sent!");
			free(ioInfo);
		}
	}
	return 0;
}

void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
