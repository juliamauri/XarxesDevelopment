#include "ModuleNetworkingClient.h"


bool  ModuleNetworkingClient::start(const char * serverAddressStr, int serverPort, const char *pplayerName)
{
	playerName = pplayerName;

	// TODO(jesus): TCP connection stuff
	// - Create the socket
	// - Create the remote address object
	// - Connect to the remote address
	// - Add the created socket to the managed list of sockets using addSocket() 
	//		* On Update() when connection is confirmed.

	LOG("Starting Client at %s@%s:%i", pplayerName, serverAddressStr, serverPort);
	LOG("Creating Client TCP Socket\n");
	if ((cSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		reportError("socket function failed with error:\n");
		return false;
	}

	// Set non-blocking socket
	LOG("Set non-blocking Client TCP Socket\n");
	u_long nonBlocking = 1;
	if (ioctlsocket(cSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
		reportError("ioctlsocket function failed with error:\n");
		return false;
	}

	//Configure Server adress
	ZeroMemory(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverAddressStr, &serverAddress.sin_addr);

	if (!ConnectToServer()) return false;

	// If everything was ok... change the state
	state = ClientState::Start;
	LOG("Waiting Server\n");

	return true;
}

bool ModuleNetworkingClient::isRunning() const
{
	return state != ClientState::Stopped;
}

bool ModuleNetworkingClient::update()
{

	if (state == ClientState::Start) {


		static bool reconnecting = false;
		static float reconnectTime = 10.f, timer = 0.f;

		// New socket set
		fd_set knowConnection;
		FD_ZERO(&knowConnection);
		FD_SET(cSocket, &knowConnection);

		// Timeout (return immediately)
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		// Select (check for connection)
		int res = select(0, nullptr, &knowConnection, nullptr, &timeout);
		if (res == SOCKET_ERROR) {
			reportError("select function failed with error:\n");
			return false;
		}

		if (FD_ISSET(cSocket, &knowConnection)) {
			LOG("Server connection accepted\n");
			addSocket(cSocket);
			// TODO(jesus): Send the player name to the server

			unsigned int nameLenght = playerName.length();
			unsigned int size = sizeof(unsigned int) + (sizeof(char) * nameLenght) + sizeof(ChatEvents);
			char* sendNameBuffer = new char[size];
			ChatEvents eventC = ChatEvents::C_USERCONNECTED;
			memcpy(&sendNameBuffer[0], &eventC, sizeof(ChatEvents));
			memcpy(&sendNameBuffer[sizeof(ChatEvents)], &nameLenght, sizeof(unsigned int));
			memcpy(&sendNameBuffer[sizeof(ChatEvents) + sizeof(unsigned int)], playerName.c_str(), sizeof(char) * nameLenght);
			send(cSocket, sendNameBuffer, size, 0);

			chatMessages.push_back("Connected To Chat");
			state = ClientState::Logging;
		}
		else if (!reconnecting) {
			LOG("Connection to the server server failed, reconnecting in %.f seconds\n", reconnectTime);
			reconnecting = true;
		}
		else {
			timer += Time.deltaTime;
			if (timer > reconnectTime) {
				ConnectToServer(reconnecting);
				timer = 0.f;
				reconnecting = false;
			}
		}
	}

	return true;
}

bool ModuleNetworkingClient::gui()
{
	if (state != ClientState::Stopped)
	{
		// NOTE(jesus): You can put ImGui code here for debugging purposes
		ImGui::Begin("Client Window");

		Texture *tex = App->modResources->client;
		ImVec2 texSize(400.0f, 400.0f * tex->height / tex->width);
		ImGui::Image(tex->shaderResource, texSize);

		ImGui::Text("%s connected to the server...", playerName.c_str());

		ImGui::End();

		//Chat Window
		ImGui::Begin("Chat Window");

		ImGui::Text((usersInChat.empty()) ? "No other user than you on chat" : "Users connected:");
		if (!usersInChat.empty()) {
			if (ImGui::CollapsingHeader("Show Users connected")) 
				for (auto& user : usersInChat) ImGui::BulletText(user.c_str());
		}

		ImGui::Separator();

		for(auto& msg : chatMessages)ImGui::TextWrapped("%s", msg.c_str());

		ImGui::Separator();
		static const uint32 messageSize = Kilobytes(1);
		static char messageDataBuffer[messageSize];
		unsigned int nameLenght = playerName.length();
		unsigned int sizeMSGAligment = (sizeof(unsigned int) * 2)  + (sizeof(char) * nameLenght) + sizeof(ChatEvents);

		ImGui::InputText("message", &messageDataBuffer[sizeMSGAligment], messageSize - sizeMSGAligment, ImGuiInputTextFlags_::ImGuiInputTextFlags_CtrlEnterForNewLine);
		ImGui::SameLine();

		unsigned int textSize = std::strlen(&messageDataBuffer[sizeMSGAligment]);
		if ((ImGui::Button("Send") || ImGui::GetIO().KeysDown[ImGuiKey_::ImGuiKey_Enter]) && textSize > 0) {
			ChatEvents eventC = ChatEvents::C_MENSAGE;
			memcpy(&messageDataBuffer[0], &eventC, sizeof(ChatEvents));
			memcpy(&messageDataBuffer[sizeof(ChatEvents)], &nameLenght, sizeof(unsigned int));
			memcpy(&messageDataBuffer[sizeof(ChatEvents) + sizeof(unsigned int)], playerName.c_str(), sizeof(char) * nameLenght);
			memcpy(&messageDataBuffer[sizeof(ChatEvents) + sizeof(unsigned int) + (sizeof(char) * nameLenght)], &textSize, sizeof(unsigned int));

			chatMessages.push_back(playerName + ": " + &messageDataBuffer[sizeMSGAligment]);
			send(cSocket, messageDataBuffer, sizeMSGAligment + textSize, 0);
			messageDataBuffer[sizeMSGAligment] = '\0';
		}

		ImGui::End();
	}

	return true;
}

void ModuleNetworkingClient::onSocketReceivedData(SOCKET socket, byte * data, uint32 recivedSize)
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
		chatMessages.push_back(fromUser + " connected to chat");
		usersInChat.push_back(fromUser);
		break;
	case ModuleNetworking::C_USERDISCONNECTED:

		for (std::vector<std::string>::iterator iter = usersInChat.begin(); iter != usersInChat.end(); iter++) {
			if (*iter == fromUser) {
				chatMessages.push_back(fromUser + " disconnected from chat");
				usersInChat.erase(iter);
				break;
			}
		}
		break;
	case ModuleNetworking::C_MENSAGE:
	{
		unsigned int messageSize = 0;
		memcpy(&messageSize, cursor, size);
		cursor += size;
		std::string message(reinterpret_cast<const char*>(cursor), messageSize);
		chatMessages.push_back(fromUser + ": " + message);
		break;
	}
	}
}

void ModuleNetworkingClient::onSocketDisconnected(SOCKET socket)
{
	LOG("Disconnected from server\n");
	App->modScreen->screenMainMenu->disconnectedFromServer = true;
	usersInChat.clear();
	chatMessages.clear();
	state = ClientState::Stopped;
}

bool ModuleNetworkingClient::ConnectToServer(bool reconnect)
{
	LOG("%s to Server\n", (reconnect) ? "Reconnecting" : "Connecting");
	if (connect(cSocket, reinterpret_cast<const sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
		DWORD error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK) //non-blocking socket throws that error: connection attempt cannot be completed immediately
		{										//with select we can check the connection
			reportError("connect function failed with error:\n", error);
			return false;
		}
	}
	return true;
}

