
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
	int addrLen;
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

list <SOCKET> socketList;

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

	void serverInit() {
		mutex = CreateMutex(NULL, FALSE, NULL);

		WSAStartup(MAKEWORD(2, 2), &wsaData); // 1.  Function Must called once until use winSock
			
		iocpObj = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);// 2. Create IOCP OBJ

		GetSystemInfo(&sysInfo);//  get system Information

		for (int i = 0; i < sysInfo.dwNumberOfProcessors; i++)  //  dwNumberOfProcessors---The number of physical processors
			_beginthreadex(NULL, 0, EchoThread, (LPVOID)iocpObj, 0, NULL);
	}

	void startListen() {
		sockInfo.socketAddr;
		sockInfo.socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);//4. Create socket  
		memset(&sockInfo.socketAddr, 0, sizeof(sockInfo.socketAddr));
		sockInfo.socketAddr.sin_family = AF_INET;
		sockInfo.socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		sockInfo.socketAddr.sin_port = htons(1234);

		bind(sockInfo.socket, (SOCKADDR*)&sockInfo.socketAddr, sizeof(sockInfo.socketAddr));
		listen(sockInfo.socket, 5);      //  5.binding  & listening
	}

};

Server server;

int main(int argc, char* argv[])
{

	
	server.serverInit();

	//BufferInfo buffInfo;


	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;
	//------------
	server.startListen();
	
								
	while (1)
	{
		SocketInfo sockInfo;
		sockInfo.addrLen = sizeof(sockInfo.socketAddr);


		sockInfo.socket = accept(server.sockInfo.socket, (SOCKADDR*)&sockInfo.socketAddr, &sockInfo.addrLen);//If new client coming, go on

		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSock = sockInfo.socket;

		WaitForSingleObject(hMutex, INFINITE);// Waiting for another thread to release the mutex

		socketList.push_back(sockInfo.socket);
		server.numOfClient++;

		ReleaseMutex(hMutex);//

		memcpy(&(handleInfo->clntAdr), &sockInfo.socketAddr, sockInfo.addrLen);

		CreateIoCompletionPort((HANDLE)sockInfo.socket, server.iocpObj, (DWORD)handleInfo, 0);// binding
																				    
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


				socketList.remove(sock);
				socketList.sort();
				server.numOfClient--;

				ReleaseMutex(hMutex);

				free(handleInfo); free(ioInfo);
				continue;
			}
			int i = 0;

			
			for each (SOCKET socket in socketList)
			{
					if (socket != sock)
					{
						LPPER_IO_DATA newioInfo;
						newioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
						memset(&(newioInfo->overlapped), 0, sizeof(OVERLAPPED));
						strncpy_s(newioInfo->buffer,ioInfo->buffer,_TRUNCATE);
						newioInfo->wsaBuf.buf = newioInfo->buffer;    //user who sent the message 
						newioInfo->wsaBuf.len = bytesTrans;
						newioInfo->rwMode = WRITE;

						WSASend(socket, &(newioInfo->wsaBuf),
							1, NULL, 0, &(newioInfo->overlapped), NULL);
					}
					else    
					{
						memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
						ioInfo->wsaBuf.len = bytesTrans;
						ioInfo->rwMode = WRITE;
						WSASend(socket, &(ioInfo->wsaBuf),
							1, NULL, 0, &(ioInfo->overlapped), NULL);
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

