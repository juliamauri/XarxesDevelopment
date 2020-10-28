#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>

//Commment and uncomment for changing TCP to UDP
#define USING_TCP


#ifndef USING_TCP
#define ITERATIONS 5
#endif
#define DEFBUFFERSIZE 512

void printWSError(const char* msg);
bool Init();
bool CleanUp();

#ifdef USING_TCP
void ListenAndAccept();
void Send(const char* msg, int msgSize);
int Recive(char* buff, int buffLenght);
#else //UDP
void Send(sockaddr* to, const char* msg, int msgSize);
int Recive(sockaddr* from, int* adrrlenght, char* buff, int buffLenght, int* msgSize);
#endif

WSADATA wsaData;
struct sockaddr_in bindAddr;
SOCKET s;
#ifdef USING_TCP
SOCKET client;
sockaddr_in clientAddr;
#endif
int port;
char error[DEFBUFFERSIZE];

int main(int argc, char* argv[]) {
	
	port = std::stoi(argv[1]);
	if (port <= 0) {
		printf("Invalid port: %i", port);
		return 1;
	}

	if (Init() == false)
	{
#ifdef USING_TCP
		ListenAndAccept();
#endif
		char recivedBuffer[DEFBUFFERSIZE];

		unsigned int count = 0;
		const char* sendmsg = "pong";
		const char* recivemsg = "ping";


		bool endConnection = false;
		while (endConnection == false) {

			bool packetRecived = false;
			int recievedLenght = 0;

#ifdef USING_TCP

			recievedLenght = Recive(recivedBuffer, DEFBUFFERSIZE);
			if (recievedLenght == 0) {
				endConnection = true;
				printf("Client Disconnected, closing Server");
			}
			else 
				printf("Bytes received: %d\n", recievedLenght);
			
#else //UDP

			struct sockaddr_in sourceSock;
			int addrLenght = sizeof(sourceSock);

			if (Recive((sockaddr*)&sourceSock, &addrLenght, recivedBuffer, DEFBUFFERSIZE, &recievedLenght) == -1)
				endConnection = true;

#endif	
			std::string recivedText(recivedBuffer, recievedLenght);
			printf("Packet readed: %s\n", recivedText.c_str());
			if (recivedText == recivemsg) {
				printf("Packet corresponds to %s, Total: %u\n", recivemsg, count);
				Sleep(500);
#ifdef USING_TCP
				Send(sendmsg, sizeof(char) * std::strlen(sendmsg));
#else //UDP
				count++;
				Send((sockaddr*)&sourceSock, sendmsg, sizeof(char) * std::strlen(sendmsg));
				if (count == ITERATIONS) break;
#endif				
			}




		}

	}
	return CleanUp();
}



bool Init() {
	bool failed = false;
	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		printWSError(error);
		failed = true;
	}

#ifdef USING_TCP
	s = socket(AF_INET, SOCK_STREAM, 0);
	printf("Starting TCP Socket\n");
#else //UDP
	s = socket(AF_INET, SOCK_DGRAM, 0);
	printf("Starting UDP Socket\n");
#endif
	
	//Configure Shockets
	ZeroMemory(&bindAddr, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(port);
	bindAddr.sin_addr.S_un.S_addr = INADDR_ANY;

	//bind socket
	int res = bind(s, (const struct sockaddr*)&bindAddr, sizeof(bindAddr));
	if (res == SOCKET_ERROR)
		failed = true;

	return failed;
}


bool CleanUp() {
	bool failed = false;

#ifdef USING_TCP
	closesocket(client);
#endif
	closesocket(s);
	
	int iResult = WSACleanup();
	if (iResult != NO_ERROR) {
		printWSError(error);
		failed = true;
	}

	return failed;
}

#ifdef USING_TCP
void ListenAndAccept() {
	printf("Waiting Client Connection\n");
	listen(s, 1);
	static int sizeAdrr = sizeof(clientAddr);
	client = accept(s, (sockaddr*)&clientAddr, &sizeAdrr);
	printf("Accepted Client Connection\n");
}

void Send(const char* msg, int msgSize) {
	send(client, msg, msgSize, 0);
	printf("Sending %s\n", msg);
}

int Recive(char* buff, int buffLenght) {
	return recv(client, buff, buffLenght, 0);
}

#else //UDP
void Send(sockaddr* to, const char* msg, int msgSize) {
	sendto(s, msg, msgSize, 0, to, sizeof(*to));
	printf("Sending %s\n", msg);
}

//return -1 END CONNECTION
//return 0 RECIEVED
int Recive(sockaddr* from, int* adrrlenght, char* buff, int buffLenght, int* msgSize) {
	int ret = 1;
	do {
		*msgSize = recvfrom(s, buff, buffLenght, 0, from, adrrlenght);
		if (*msgSize > 0) {
			printf("Bytes received: %d\n", *msgSize);
			ret = 0;
		}
		else if (*msgSize == SOCKET_ERROR) {
			printWSError(error);
			ret = -1;
		}

	} while (ret != 0 && ret != -1);

	return ret;
}
#endif

void printWSError(const char* msg)
{
	wchar_t* s = NULL;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&s,
		0, NULL);
	fprintf(stderr, "%s: %S\n", msg, s);
	LocalFree(s);
}