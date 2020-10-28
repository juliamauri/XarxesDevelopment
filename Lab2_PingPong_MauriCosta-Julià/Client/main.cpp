#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>

//Commment and uncomment for changing TCP to UDP
#define USING_TCP

#define ITERATIONS 5
#define DEFBUFFERSIZE 512

void printWSError(const char* msg);
bool Init();
bool CleanUp();

#ifdef USING_TCP
void Connect(sockaddr* to);
void Send(const char* msg, int msgSize);
int Recive(char* buff, int buffLenght);
#else //UDP
void Send(sockaddr* to, const char* msg, int msgSize);
int Recive(sockaddr* from, int* adrrlenght, char* buff, int buffLenght, int* msgSize);
#endif

WSADATA wsaData;
struct sockaddr_in remoteAddr;
SOCKET s;
int port;
char* ipRemote;
char error[DEFBUFFERSIZE];

int main(int argc, char* argv[]) {
	ipRemote = argv[1];
	port = std::stoi(argv[2]);
	if (port <= 0) {
		printf("Invalid port: %i", port);
		return 1;
	}

	if (Init() == false)
	{
#ifdef USING_TCP
		Connect((sockaddr*)&remoteAddr);
#endif
		char recivedBuffer[DEFBUFFERSIZE];
		int recievedLenght = 0;

		unsigned int count = 0;
		const char* sendmsg = "ping";
		const char* recivemsg = "pong";

		bool endConnection = false;
		while (count < ITERATIONS && endConnection == false) {

#ifdef USING_TCP
			Send(sendmsg, sizeof(char) * std::strlen(sendmsg));

			recievedLenght = Recive(recivedBuffer, DEFBUFFERSIZE);
			if (recievedLenght == 0) {
				endConnection = true;
				printf("Server disconnected, clossing client");
			}
			else
				printf("Bytes received: %d\n", recievedLenght);

#else //UDP
			Send((sockaddr*)&remoteAddr, sendmsg, sizeof(char) * std::strlen(sendmsg));

			bool packetRecived = false;
			sockaddr sourcePacket;
			int sourcePacketLenght = sizeof(sourcePacket);
			if (Recive(&sourcePacket, &sourcePacketLenght, recivedBuffer, DEFBUFFERSIZE, &recievedLenght) == -1)
				endConnection = true;

#endif	
			if (!endConnection) {
				std::string recivedText(recivedBuffer, recievedLenght);
				printf("Packet readed: %s\n", recivedText.c_str());
				if (recivedText == recivemsg) {
					count++;
					printf("Packet corresponds to %s, Total: %u\n", recivemsg, count);
					Sleep(500);
				}
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
#else
	s = socket(AF_INET, SOCK_DGRAM, 0);
	printf("Starting UDP Socket\n");
#endif
	if (s == INVALID_SOCKET) {
		printWSError(error);
		failed = true;
	}

	//Configure Shockets
	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipRemote, &remoteAddr.sin_addr);

	return failed;
}


bool CleanUp() {
	bool failed = false;

	closesocket(s);

	int iResult = WSACleanup();
	if (iResult != NO_ERROR) {
		printWSError(error);
		failed = true;
	}

	return failed;
}


#ifdef USING_TCP

void Connect(sockaddr* to) {
	connect(s, to, sizeof(*to));
	printf("Client Connected to Server\n");
}
void Send(const char* msg, int msgSize) {
	send(s, msg, msgSize, 0);
	printf("Sending %s\n", msg);
}

int Recive(char* buff, int buffLenght) {
	return recv(s, buff, buffLenght, 0);
}

#else
void Send(sockaddr* to, const char* msg, int msgSize) {
	sendto(s, msg, msgSize, 0, (sockaddr*)to, sizeof(*to));
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