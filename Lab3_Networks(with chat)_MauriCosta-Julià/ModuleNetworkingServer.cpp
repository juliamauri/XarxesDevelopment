#include "ModuleNetworkingServer.h"

#include <string>

//////////////////////////////////////////////////////////////////////
// ModuleNetworkingServer public methods
//////////////////////////////////////////////////////////////////////

bool ModuleNetworkingServer::start(int port)
{
	// TODO(jesus): TCP listen socket stuff
	// - Create the listenSocket
	// - Set address reuse
	// - Bind the socket to a local interface
	// - Enter in listen mode
	// - Add the listenSocket to the managed list of sockets using addSocket()

	LOG("Creating Server TCP Socket at port %i\n", port);
	if ((listenSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) 
	{
		reportError("socket function failed with error:\n");
		return false;
	}

	// Set non-blocking socket
	LOG("Set non-blocking Server TCP Socket\n");
	u_long nonBlocking = 1;
	if (ioctlsocket(listenSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
		reportError("ioctlsocket function failed with error:\n");
		return false;
	}

	//Configure Shocket
	sockaddr_in initInfo;
	ZeroMemory(&initInfo, sizeof(initInfo));
	initInfo.sin_family = AF_INET;
	initInfo.sin_port = htons(port);
	initInfo.sin_addr.S_un.S_addr = INADDR_ANY;

	LOG("Binding server socket\n");
	if (bind(listenSocket, (const struct sockaddr*)&initInfo, sizeof(initInfo)) == SOCKET_ERROR)
	{
		reportError("bind function failed with error:\n");
		return false;
	}
	addSocket(listenSocket);

	LOG("Listen server socket\n");
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		reportError("listen function failed with error:\n");
		return false;
	}

	state = ServerState::Listening;

	return true;
}

bool ModuleNetworkingServer::isRunning() const
{
	return state != ServerState::Stopped;
}



//////////////////////////////////////////////////////////////////////
// Module virtual methods
//////////////////////////////////////////////////////////////////////

bool ModuleNetworkingServer::update()
{
	return true;
}

bool ModuleNetworkingServer::gui()
{
	if (state != ServerState::Stopped)
	{
		// NOTE(jesus): You can put ImGui code here for debugging purposes
		ImGui::Begin("Server Window");

		Texture *tex = App->modResources->server;
		ImVec2 texSize(400.0f, 400.0f * tex->height / tex->width);
		ImGui::Image(tex->shaderResource, texSize);

		if (ImGui::Button("Close Server")) {
			disconnect();
			state = ServerState::Stopped;
			connectedSockets.clear();
		}

		ImGui::Text("List of connected sockets:");
		static bool deletionSelected = false;
		static std::vector<ConnectedSocket>::iterator toDelete;
		toDelete = connectedSockets.begin();
		unsigned int count = 0;
		for (auto &connectedSocket : connectedSockets)
		{
			ImGui::Separator();
			ImGui::Text("Socket ID: %d", connectedSocket.socket);
			ImGui::Text("Address: %d.%d.%d.%d:%d",
				connectedSocket.address.sin_addr.S_un.S_un_b.s_b1,
				connectedSocket.address.sin_addr.S_un.S_un_b.s_b2,
				connectedSocket.address.sin_addr.S_un.S_un_b.s_b3,
				connectedSocket.address.sin_addr.S_un.S_un_b.s_b4,
				ntohs(connectedSocket.address.sin_port));
			ImGui::Text("Player name: %s", connectedSocket.playerName.c_str());

			ImGui::PushID(std::string("#" + std::to_string(count++) + "socketDelete").c_str());
			if (ImGui::Button("Kick form server")) deletionSelected = true;
			else if(!deletionSelected) toDelete++;
			ImGui::PopID();
		}

		if (deletionSelected) {
			auto& socketToDelete = *toDelete;

			//Tell client that was kicked
			unsigned int sizeToSend = sizeof(ChatEvents);
			char* sendKickUserBuffer = new char[sizeToSend];
			ChatEvents eventC = ChatEvents::C_SERVERKICK;
			memcpy(&sendKickUserBuffer[0], &eventC, sizeof(ChatEvents));
			send(socketToDelete.socket, sendKickUserBuffer, sizeToSend, 0);
			DisconnectSocket(socketToDelete.socket);

			//Tell other clients that one client was disconnected.
			unsigned int nameLenght = socketToDelete.playerName.length();
			sizeToSend = sizeof(unsigned int) + (sizeof(char) * nameLenght) + sizeof(ChatEvents);
			char* sendCurrentUsersBuffer = new char[sizeToSend];
			eventC = ChatEvents::C_USERCONNECTED;
			memcpy(&sendCurrentUsersBuffer[0], &eventC, sizeof(ChatEvents));
			memcpy(&sendCurrentUsersBuffer[sizeof(ChatEvents)], &nameLenght, sizeof(unsigned int));
			memcpy(&sendCurrentUsersBuffer[sizeof(ChatEvents) + sizeof(unsigned int)], socketToDelete.playerName.c_str(), sizeof(char) * nameLenght);

			connectedSockets.erase(toDelete);
			for (auto& connectedSocket : connectedSockets)	
				send(connectedSocket.socket, sendCurrentUsersBuffer, sizeToSend, 0);

			deletionSelected = false;
		}

		ImGui::End();
	}

	return true;
}



//////////////////////////////////////////////////////////////////////
// ModuleNetworking virtual methods
//////////////////////////////////////////////////////////////////////

bool ModuleNetworkingServer::isListenSocket(SOCKET socket) const
{
	return socket == listenSocket;
}

void ModuleNetworkingServer::onSocketConnected(SOCKET socket, const sockaddr_in &socketAddress)
{
LOG("New socket connection detected\n");
// Add a new connected socket to the list
ConnectedSocket connectedSocket;
connectedSocket.socket = socket;
connectedSocket.address = socketAddress;
connectedSockets.push_back(connectedSocket);
addSocket(socket);
}

void ModuleNetworkingServer::onSocketReceivedData(SOCKET socket, byte* data, uint32 recivedSize)
{
	ChatEvents incomingEvent;
	unsigned int size = sizeof(ChatEvents);
	byte* cursor = data;
	memcpy(&incomingEvent, cursor, size);
	cursor += size;

	unsigned int nameSize = 0;
	size = sizeof(unsigned int);
	memcpy(&nameSize, cursor, size);
	cursor += size;

	std::string fromUser(reinterpret_cast<const char*>(cursor), nameSize);
	cursor += nameSize * sizeof(char);

	switch (incomingEvent)
	{
	case ModuleNetworking::C_USERCONNECTED:
		// Set the player name of the corresponding connected socket proxy
		for (auto& connectedSocket : connectedSockets)
		{
			if (connectedSocket.socket == socket)
			{
				connectedSocket.playerName = fromUser;
				LOG("%s connected on server.\n", connectedSocket.playerName.c_str());
			}
			else {
				send(connectedSocket.socket, reinterpret_cast<const char*>(data), recivedSize, 0);

				unsigned int nameLenght = connectedSocket.playerName.length();
				unsigned int sizeToSend = sizeof(unsigned int) + (sizeof(char) * nameLenght) + sizeof(ChatEvents);
				char* sendCurrentUsersBuffer = new char[sizeToSend];
				ChatEvents eventC = ChatEvents::C_USERCONNECTED;
				memcpy(&sendCurrentUsersBuffer[0], &eventC, sizeof(ChatEvents));
				memcpy(&sendCurrentUsersBuffer[sizeof(ChatEvents)], &nameLenght, sizeof(unsigned int));
				memcpy(&sendCurrentUsersBuffer[sizeof(ChatEvents) + sizeof(unsigned int)], connectedSocket.playerName.c_str(), sizeof(char) * nameLenght);

				send(socket, sendCurrentUsersBuffer, sizeToSend, 0);
			}
		}
		break;
	case ModuleNetworking::C_MENSAGE:
		std::string message(reinterpret_cast<const char*>(cursor));
		LOG("Log message -> %s: %s", fromUser.c_str(), message.c_str());
		for (auto& connectedSocket : connectedSockets)
		{
			if (connectedSocket.socket != socket)
			{
				send(connectedSocket.socket, reinterpret_cast<const char*>(data), recivedSize, 0);
			}
		}
		break;
	}


}

void ModuleNetworkingServer::onSocketDisconnected(SOCKET socket)
{
	std::string playerDisconnected;
	// Remove the connected socket from the list
	for (auto it = connectedSockets.begin(); it != connectedSockets.end(); ++it)
	{
		auto& connectedSocket = *it;
		if (connectedSocket.socket == socket)
		{
			playerDisconnected = connectedSocket.playerName;
			WLOG("%s disconnected from server.\n", connectedSocket.playerName.c_str());
			connectedSockets.erase(it);
			break;
		}
	}

	if (!playerDisconnected.empty()) {
		unsigned int nameLenght = playerDisconnected.length();
		unsigned int size = sizeof(unsigned int) + (sizeof(char) * nameLenght) + sizeof(ChatEvents);
		char* sendNameDisconnectedBuffer = new char[size];
		ChatEvents eventC = ChatEvents::C_USERDISCONNECTED;
		memcpy(&sendNameDisconnectedBuffer[0], &eventC, sizeof(ChatEvents));
		memcpy(&sendNameDisconnectedBuffer[sizeof(ChatEvents)], &nameLenght, sizeof(unsigned int));
		memcpy(&sendNameDisconnectedBuffer[sizeof(ChatEvents) + sizeof(unsigned int)], playerDisconnected.c_str(), sizeof(char) * nameLenght);

		for (auto it = connectedSockets.begin(); it != connectedSockets.end(); ++it)
			send((*it).socket, sendNameDisconnectedBuffer, size, 0);
	}
}

void ModuleNetworkingServer::DisconnectSocket(SOCKET s)
{
	for (auto iter = sockets.begin(); iter != sockets.end(); iter++)
		if (*iter == s){
			shutdown(s, 2);
			closesocket(s);
			sockets.erase(iter);
			break;
		}
}

