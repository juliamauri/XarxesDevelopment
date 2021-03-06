#include "ModuleNetworkingClient.h"


//////////////////////////////////////////////////////////////////////
// ModuleNetworkingClient public methods
//////////////////////////////////////////////////////////////////////


void ModuleNetworkingClient::setServerAddress(const char * pServerAddress, uint16 pServerPort)
{
	serverAddressStr = pServerAddress;
	serverPort = pServerPort;
}

void ModuleNetworkingClient::setPlayerInfo(const char * pPlayerName, uint8 pSpaceshipType)
{
	playerName = pPlayerName;
	spaceshipType = pSpaceshipType;
}

void ModuleNetworkingClient::clear()
{
	replicationClient.clear();
	deliveryManager.clear();
}



//////////////////////////////////////////////////////////////////////
// ModuleNetworking virtual methods
//////////////////////////////////////////////////////////////////////

void ModuleNetworkingClient::onStart()
{
	if (!createSocket()) return;

	if (!bindSocketToPort(0)) {
		disconnect();
		return;
	}

	// Create remote address
	serverAddress = {};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(serverPort);
	int res = inet_pton(AF_INET, serverAddressStr.c_str(), &serverAddress.sin_addr);
	if (res == SOCKET_ERROR) {
		reportError("ModuleNetworkingClient::startClient() - inet_pton");
		disconnect();
		return;
	}

	state = ClientState::Connecting;

	inputDataFront = 0;
	inputDataBack = 0;

	secondsSinceLastHello = 9999.0f;
	secondsSinceLastInputDelivery = 0.0f;
}

void ModuleNetworkingClient::onGui()
{
	if (state == ClientState::Stopped) return;

	if (ImGui::CollapsingHeader("ModuleNetworkingClient", ImGuiTreeNodeFlags_DefaultOpen))
	{
		switch (state)
		{
		case ModuleNetworkingClient::ClientState::Connecting:
			ImGui::Text("Connecting to server...");
			break;
		case ModuleNetworkingClient::ClientState::Connected:
			ImGui::Text("Connected to server");

			ImGui::Separator();

			ImGui::Text("Player info:");
			ImGui::Text(" - Id: %u", playerId);
			ImGui::Text(" - Name: %s", playerName.c_str());

			ImGui::Separator();

			ImGui::Text("Spaceship info:");
			ImGui::Text(" - Type: %u", spaceshipType);
			ImGui::Text(" - Network id: %u", networkId);

			{
				vec2 playerPosition = { 0.0f, 0.0f };
				if (networkId > 0) {
					GameObject* playerGameObject = App->modLinkingContext->getNetworkGameObject(networkId);
					if (playerGameObject != nullptr) {
						playerPosition = playerGameObject->position;
					}

				}
				ImGui::Text(" - Coordinates: (%f, %f)", playerPosition.x, playerPosition.y);
			}

			ImGui::Separator();

			ImGui::Text("Connection checking info:");
			ImGui::Text(" - Ping interval (s): %f", PING_INTERVAL_SECONDS);
			ImGui::Text(" - Disconnection timeout (s): %f", DISCONNECT_TIMEOUT_SECONDS);

			ImGui::Separator();

			ImGui::Text("Input:");
			ImGui::InputFloat("Delivery interval (s)", &inputDeliveryIntervalSeconds, 0.01f, 0.1f, 4);
			break;
		}
	}
}

void ModuleNetworkingClient::onPacketReceived(const InputMemoryStream &packet, const sockaddr_in &fromAddress)
{

	uint32 protoId;
	packet >> protoId;
	if (protoId != PROTOCOL_ID) return;

	ServerMessage message;
	packet >> message;

	switch (state)
	{
	case ModuleNetworkingClient::ClientState::Connecting:
		switch (message)
		{
		case ServerMessage::Welcome:
			packet >> playerId;
			packet >> networkId;

			LOG("ModuleNetworkingClient::onPacketReceived() - Welcome from server");
			state = ClientState::Connected;
			break;
		case ServerMessage::Unwelcome:
			WLOG("ModuleNetworkingClient::onPacketReceived() - Unwelcome from server :-(");
			disconnect();
			break;
		}

		break;
	case ModuleNetworkingClient::ClientState::Connected:
		// TODO(you): UDP virtual connection lab session
		//Client updates last time ping recived.
		switch (message)
		{
		case ServerMessage::Ping:
			secondsSinceServerLastPing = 0.0f;
			break;
		case ServerMessage::InputConfirmation:
			packet >> inputDataFront;
			break;
		case ServerMessage::WorldDelivery:
			// TODO(you): Reliability on top of UDP lab session
			if (deliveryManager.processSequencerNumber(packet, timeToResetNextExpected))
				// TODO(you): World state replication lab session
				replicationClient.read(packet);
			break;
		case ServerMessage::UpdateScoreBoard:
			App->modScreen->screenGame->onPacketRecieved(packet);
			break;
		case ServerMessage::YourNetID:
			packet >> networkId;
			break;
		}
		break;
	}
}

void ModuleNetworkingClient::onUpdate()
{
	switch (state)
	{
	case ModuleNetworkingClient::ClientState::Stopped: return;
	case ModuleNetworkingClient::ClientState::Connecting:
		secondsSinceLastHello += Time.deltaTime;

		if (secondsSinceLastHello > 0.1f)
		{
			secondsSinceLastHello = 0.0f;

			OutputMemoryStream packet;
			packet << PROTOCOL_ID;
			packet << ClientMessage::Hello;
			packet << spaceshipType;
			packet << playerName;

			sendPacket(packet, serverAddress);
		}
		break;
	case ModuleNetworkingClient::ClientState::Connected:
		timeToResetNextExpected += Time.deltaTime;

		// TODO(you): UDP virtual connection lab session
		// Suma el temps de last packet recieved, si supera el timeout es desconecta.
		// DISCONNECT_TIMEOUT_SECONDS

		secondsSinceServerLastPing += Time.deltaTime;

		if (secondsSinceServerLastPing > DISCONNECT_TIMEOUT_SECONDS)
		{
			secondsSinceServerLastPing = 0.0f;

			state = ClientState::Stopped;
			disconnect();
			return;
		}

		// TODO(you): UDP virtual connection lab session
		// Suma el temps del ultim ping enviat, si es supera, envia un ping.
		// PING_INTERVAL_SECONDS

		secondsSinceLastPingDelivered += Time.deltaTime;

		if (secondsSinceLastPingDelivered > PING_INTERVAL_SECONDS)
		{
			secondsSinceLastPingDelivered = 0.0f;

			OutputMemoryStream PingPacket;
			PingPacket << PROTOCOL_ID;
			PingPacket << ServerMessage::Ping;
			sendPacket(PingPacket, serverAddress);

			WLOG("Ping sent to server.");
		}

		// Process more inputs if there's space
		if (inputDataBack - inputDataFront < ArrayCount(inputData))
		{
			// Pack current input
			uint32 currentInputData = inputDataBack++;
			InputPacketData& inputPacketData = inputData[currentInputData % ArrayCount(inputData)];
			inputPacketData.sequenceNumber = currentInputData;
			inputPacketData.horizontalAxis = Input.horizontalAxis;
			inputPacketData.verticalAxis = Input.verticalAxis;
			inputPacketData.buttonBits = packInputControllerButtons(Input);
		}

		secondsSinceLastInputDelivery += Time.deltaTime;

		// Input delivery interval timed out: create a new input packet
		if (secondsSinceLastInputDelivery > inputDeliveryIntervalSeconds && App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running)
		{
			secondsSinceLastInputDelivery = 0.0f;

			OutputMemoryStream packet;
			packet << PROTOCOL_ID;
			packet << ClientMessage::Input;

			// TODO(you): Input Part | Reliability on top of UDP lab session

			for (uint32 i = inputDataFront; i < inputDataBack; ++i)
			{
				InputPacketData& inputPacketData = inputData[i % ArrayCount(inputData)];
				packet << inputPacketData.sequenceNumber;
				packet << inputPacketData.horizontalAxis;
				packet << inputPacketData.verticalAxis;
				packet << inputPacketData.buttonBits;
			}

			sendPacket(packet, serverAddress);
		}

		if (deliveryManager.hasSequenceNumberPendingAck()) {
			OutputMemoryStream packet;
			packet << PROTOCOL_ID;
			packet << ClientMessage::DeliveryConfirmation;
			deliveryManager.writeSequenceNumbersPendingAck(packet);
			sendPacket(packet, serverAddress);
		}


		// TODO(you): Latency management lab session

		if (networkId > 0) {
			// Update camera for player
			GameObject* playerGameObject = App->modLinkingContext->getNetworkGameObject(networkId);
			if (playerGameObject != nullptr)
			{
				App->modRender->cameraPosition = playerGameObject->position;
			}
		}
		break;
	}
}

void ModuleNetworkingClient::onConnectionReset(const sockaddr_in & fromAddress)
{
	disconnect();
}

void ModuleNetworkingClient::onDisconnect()
{
	state = ClientState::Stopped;

	GameObject *networkGameObjects[MAX_NETWORK_OBJECTS] = {};
	uint16 networkGameObjectsCount;
	App->modLinkingContext->getNetworkGameObjects(networkGameObjects, &networkGameObjectsCount);
	App->modLinkingContext->clear();

	for (uint32 i = 0; i < networkGameObjectsCount; ++i)
	{
		Destroy(networkGameObjects[i]);
	}

	App->modRender->cameraPosition = {};
}
