
//#define _CRT_SECURE_NO_WARNINGS

//--------------------Compile Works at X86, 

#include "stdafx.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <fstream>
#include <stdio.h>  
#include <stdlib.h>  
#include <process.h>  

#include <winsock2.h>  

#include <windows.h>  

#include <iostream>
#include <list>

#include <time.h>
#include <string>

#pragma comment(lib,"ws2_32.lib")

#define BUF_SIZE 1000  
#define READ    3  
#define WRITE   5  
#define NAME_SIZE 20
#define MESSAGE_SIZE 100

using namespace std;

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

enum MessageType {
	req_con, req_discon, send_mes
};

#pragma pack(push,1)
typedef struct PACKAGE {
	//int nameLength;
	MessageType msgType;
	char message[MESSAGE_SIZE];
}PACKAGE;
#pragma pack(pop)


unsigned int  WINAPI EchoThreadMain(LPVOID CompletionPortIO);

int clientcount = 0;
HANDLE hMutex;//

list <SOCKET> socketList;

const std::string currentDateTime() {
	time_t     now = time(0); //
	struct tm  tstruct;
	char       buf[80];
	localtime_s(&tstruct,&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct); // YYYY-MM-DD.

	return buf;
}

int main(int argc, char* argv[])
{

	hMutex = CreateMutex(NULL, FALSE, NULL);//mutex initializing

	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	int  i;
	DWORD recvBytes = 0, flags = 0;

	WSAStartup(MAKEWORD(2, 2), &wsaData);  // 1.  Function Must called once until use winSock

	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);// 2. Create IOCP OBJ
	GetSystemInfo(&sysInfo);//  get system Information

	for (i = 0; i < sysInfo.dwNumberOfProcessors; i++)  //  dwNumberOfProcessors---The number of physical processors
		_beginthreadex(NULL, 0, EchoThreadMain, (LPVOID)hComPort, 0, NULL);
															//  3. Create N thread
	
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);//4. Create socket  
	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(1234);

	bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr));
	listen(hServSock, 5);      //  5.binding  & listening
								
	while (1)
	{
		SOCKET hClntSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);

		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &addrLen);//If new client coming, go on

		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSock = hClntSock;

		WaitForSingleObject(hMutex, INFINITE);// Waiting for another thread to release the mutex

		socketList.push_back(hClntSock);
		clientcount++;
		
		ReleaseMutex(hMutex);

		memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

		CreateIoCompletionPort((HANDLE)hClntSock, hComPort, (DWORD)handleInfo, 0);// binding
																				    
		ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));  //Reset Buffer ,Wait for Message
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->rwMode = READ;

		WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf),
			1, &recvBytes, &flags, &(ioInfo->overlapped), NULL);
	}



	CloseHandle(hMutex);//  Destroy Mutex
	return 0;
}

unsigned int WINAPI EchoThreadMain(LPVOID pComPort)
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
				
				clientcount--;
				

				ReleaseMutex(hMutex);

				free(handleInfo); free(ioInfo);
				continue;
			}

			PACKAGE * pPack;
			pPack = (PACKAGE *)ioInfo->buffer;

			switch (pPack->msgType) {
			case MessageType::req_con:
				{

				string str = currentDateTime()+" "+pPack->message+" connected";
				cout << str << endl;
				ofstream outFile("serverLog.txt", ios::app);

				outFile << str << endl;
				outFile.close();

				//-------------------  TEST
				
				//-------------------

				for each (SOCKET socket in socketList)
				{
						LPPER_IO_DATA newioInfo;
						newioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
						memset(&(newioInfo->overlapped), 0, sizeof(OVERLAPPED));

						char dest[MESSAGE_SIZE + NAME_SIZE];
						dest[0] = '\0';
						strcat_s(dest, sizeof(dest), pPack->message);
						strcat_s(dest, sizeof(dest), " connected");

						PACKAGE sendPack;
						PACKAGE* psendPack = &sendPack;
						sendPack.msgType = MessageType::req_con;
						strcpy_s(sendPack.message, MESSAGE_SIZE , dest);

						memcpy_s(newioInfo->buffer, 1000, psendPack, sizeof(sendPack));

						newioInfo->wsaBuf.buf = newioInfo->buffer;    //user who sent the message 
					
						newioInfo->wsaBuf.len = 4+strlen(sendPack.message)+1;
						newioInfo->rwMode = WRITE;

						WSASend(socket, &(newioInfo->wsaBuf),
							1, NULL, 0, &(newioInfo->overlapped), NULL);
					
				}

				}

				break;
			case MessageType::req_discon:
			{
				//string show;
				cout << "\n ip:" << inet_ntoa(handleInfo->clntAdr.sin_addr) << endl;
				cout <<"\n port:"<< (int)ntohs(handleInfo->clntAdr.sin_port) << endl;
				string str = currentDateTime() + " " + pPack->message + " disconnected";

				ofstream outFile("serverLog.txt", ios::app);

				outFile << str << endl;
				outFile.close();

				//-------------------  TEST
				cout << str << endl;
				//-------------------


				for each (SOCKET socket in socketList)
				{
					LPPER_IO_DATA newioInfo;
					newioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
					memset(&(newioInfo->overlapped), 0, sizeof(OVERLAPPED));

					char dest[MESSAGE_SIZE + NAME_SIZE];
					dest[0] = '\0';
					strcat_s(dest, sizeof(dest), pPack->message);
					strcat_s(dest, sizeof(dest), " disconnected");

					PACKAGE sendPack;
					PACKAGE* psendPack = &sendPack;
					sendPack.msgType = MessageType::req_discon;
					strcpy_s(sendPack.message, MESSAGE_SIZE, dest);

					memcpy_s(newioInfo->buffer, 1000, psendPack, sizeof(sendPack));

					newioInfo->wsaBuf.buf = newioInfo->buffer;    //user who sent the message 

					newioInfo->wsaBuf.len = 4 + strlen(sendPack.message) + 1;
					newioInfo->rwMode = WRITE;

					WSASend(socket, &(newioInfo->wsaBuf),
						1, NULL, 0, &(newioInfo->overlapped), NULL);

				}
				//------------  May cause problem
				WaitForSingleObject(hMutex, INFINITE);

				closesocket(sock);

				socketList.remove(sock);
				socketList.sort();

				clientcount--;


				ReleaseMutex(hMutex);

				free(handleInfo); free(ioInfo);
				continue;
				//-------------
			}
				break;
			case MessageType::send_mes:
			{
				for each (SOCKET socket in socketList)
				{
					LPPER_IO_DATA newioInfo;
					newioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
					memset(&(newioInfo->overlapped), 0, sizeof(OVERLAPPED));

					char dest[MESSAGE_SIZE + NAME_SIZE];
					dest[0] = '\0';
					strcat_s(dest, sizeof(dest), pPack->message);
					
					PACKAGE sendPack;
					PACKAGE* psendPack = &sendPack;
					sendPack.msgType = MessageType::send_mes;
					strcpy_s(sendPack.message, MESSAGE_SIZE, dest);

					memcpy_s(newioInfo->buffer, 1000, psendPack, sizeof(sendPack));

					newioInfo->wsaBuf.buf = newioInfo->buffer;    //user who sent the message 

					newioInfo->wsaBuf.len = 4 + strlen(sendPack.message) + 1;
					newioInfo->rwMode = WRITE;

					WSASend(socket, &(newioInfo->wsaBuf),
						1, NULL, 0, &(newioInfo->overlapped), NULL);
					//free(newioInfo);
				}
			}
				break;
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
