#include "ModuleNetworkingServer.h"



//////////////////////////////////////////////////////////////////////
// ModuleNetworkingServer public methods
//////////////////////////////////////////////////////////////////////

void ModuleNetworkingServer::setListenPort(int port)
{
	listenPort = port;
}

void ModuleNetworkingServer::RespawnPlayers()
{
	for (ClientProxy& clientProxy : clientProxies)
	{
		if (clientProxy.connected)
		{
			clientProxy.respawn = 0.0f;

			if (!clientProxy.gameObject) {
				// Create new network object
				vec2 initialPosition = 500.0f * vec2{ Random.next() - 0.5f, Random.next() - 0.5f };
				float initialAngle = 360.0f * Random.next();
				clientProxy.gameObject = spawnPlayer(clientProxy.spaceShipType, initialPosition, initialAngle);

				// SendNetworkID to player
				OutputMemoryStream netIDPacket;
				netIDPacket << PROTOCOL_ID;
				netIDPacket << ServerMessage::YourNetID;
				netIDPacket << clientProxy.gameObject->networkId;
				sendPacket(netIDPacket, clientProxy.address);

				// Send all network objects to the new player
				uint16 networkGameObjectsCount;
				GameObject* networkGameObjects[MAX_NETWORK_OBJECTS];
				App->modLinkingContext->getNetworkGameObjects(networkGameObjects, &networkGameObjectsCount);
				for (uint16 i = 0; i < networkGameObjectsCount; ++i)
				{
					GameObject* gameObject = networkGameObjects[i];

					// TODO(you): World state replication lab session
					if (gameObject->networkId == clientProxy.gameObject->networkId) continue;
					clientProxy.replicationServer.create(gameObject->networkId);
				}
			}
			else {
				vec2 newPosition = 500.0f * vec2{ Random.next() - 0.5f, Random.next() - 0.5f };
				float newAngle = 360.0f * Random.next();
				clientProxy.gameObject->position.x = newPosition.x;
				clientProxy.gameObject->position.y = newPosition.y;
				clientProxy.gameObject->angle = newAngle;
				dynamic_cast<Spaceship*>(clientProxy.gameObject->behaviour)->hitPoints = Spaceship::MAX_HIT_POINTS;

				uint16 networkGameObjectsCount;
				GameObject* networkGameObjects[MAX_NETWORK_OBJECTS];
				App->modLinkingContext->getNetworkGameObjects(networkGameObjects, &networkGameObjectsCount);
				for (uint16 i = 0; i < networkGameObjectsCount; ++i)
				{
					GameObject* gameObject = networkGameObjects[i];
					clientProxy.replicationServer.update(gameObject->networkId);
				}
			}
		}
	}
}

void ModuleNetworkingServer::AddPoint(GameObject* toAdd)
{
	for (ClientProxy& clientProxy : clientProxies)
		if (clientProxy.connected)
			if (clientProxy.gameObject == toAdd) {
				App->modScreen->screenGame->ScorePoint(clientProxy.scoreBoardID);
				return;
			}
}

//////////////////////////////////////////////////////////////////////
// ModuleNetworking virtual methods
//////////////////////////////////////////////////////////////////////

void ModuleNetworkingServer::onStart()
{
	if (!createSocket()) return;

	// Reuse address
	int enable = 1;
	int res = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int));
	if (res == SOCKET_ERROR) {
		reportError("ModuleNetworkingServer::start() - setsockopt");
		disconnect();
		return;
	}

	// Create and bind to local address
	if (!bindSocketToPort(listenPort)) {
		return;
	}

	state = ServerState::Listening;
}

void ModuleNetworkingServer::onGui()
{
	if (ImGui::CollapsingHeader("Match Controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("End match")) App->modScreen->screenGame->EndMatch();
	}

	if (ImGui::CollapsingHeader("ModuleNetworkingServer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Connection checking info:");
		ImGui::Text(" - Ping interval (s): %f", PING_INTERVAL_SECONDS);
		ImGui::Text(" - Disconnection timeout (s): %f", DISCONNECT_TIMEOUT_SECONDS);

		ImGui::Separator();

		if (state == ServerState::Listening)
		{
			int count = 0;

			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (clientProxies[i].connected)
				{
					ImGui::Text("CLIENT %d", count++);
					ImGui::Text(" - address: %d.%d.%d.%d",
						clientProxies[i].address.sin_addr.S_un.S_un_b.s_b1,
						clientProxies[i].address.sin_addr.S_un.S_un_b.s_b2,
						clientProxies[i].address.sin_addr.S_un.S_un_b.s_b3,
						clientProxies[i].address.sin_addr.S_un.S_un_b.s_b4);
					ImGui::Text(" - port: %d", ntohs(clientProxies[i].address.sin_port));
					ImGui::Text(" - name: %s", clientProxies[i].name.c_str());
					ImGui::Text(" - id: %d", clientProxies[i].clientId);
					if (clientProxies[i].gameObject != nullptr)
					{
						ImGui::Text(" - gameObject net id: %d", clientProxies[i].gameObject->networkId);
					}
					else
					{
						ImGui::Text(" - gameObject net id: (null)");
					}
					
					ImGui::Separator();
				}
			}

			ImGui::Checkbox("Render colliders", &App->modRender->mustRenderColliders);
		}
	}
}

void ModuleNetworkingServer::onPacketReceived(const InputMemoryStream &packet, const sockaddr_in &fromAddress)
{
	if (state == ServerState::Listening)
	{
		uint32 protoId;
		packet >> protoId;
		if (protoId != PROTOCOL_ID) return;

		ClientMessage message;
		packet >> message;

		ClientProxy *proxy = getClientProxy(fromAddress);

		if (message == ClientMessage::Hello)
		{
			if (proxy == nullptr)
			{
				proxy = createClientProxy();

				if (proxy != nullptr)
				{
					packet >> proxy->spaceShipType;
					std::string playerName;
					packet >> playerName;

					proxy->address.sin_family = fromAddress.sin_family;
					proxy->address.sin_addr.S_un.S_addr = fromAddress.sin_addr.S_un.S_addr;
					proxy->address.sin_port = fromAddress.sin_port;
					proxy->connected = true;
					proxy->name = playerName;
					proxy->clientId = nextClientId++;

					proxy->scoreBoardID = App->modScreen->screenGame->AddPlayer(playerName.c_str(), proxy->spaceShipType);

					if (App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running) {
						// Create new network object
						vec2 initialPosition = 500.0f * vec2{ Random.next() - 0.5f, Random.next() - 0.5f};
						float initialAngle = 360.0f * Random.next();
						proxy->gameObject = spawnPlayer(proxy->spaceShipType, initialPosition, initialAngle);
					}

				}
			}

			if (proxy != nullptr)
			{
				// Send welcome to the new player
				OutputMemoryStream welcomePacket;
				welcomePacket << PROTOCOL_ID;
				welcomePacket << ServerMessage::Welcome;
				welcomePacket << proxy->clientId;
				if (App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running)
					welcomePacket << proxy->gameObject->networkId;
				else
					welcomePacket << (uint32)0;
				sendPacket(welcomePacket, fromAddress);

				if (App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running) {

					// Send all network objects to the new player
					uint16 networkGameObjectsCount;
					GameObject* networkGameObjects[MAX_NETWORK_OBJECTS];
					App->modLinkingContext->getNetworkGameObjects(networkGameObjects, &networkGameObjectsCount);
					for (uint16 i = 0; i < networkGameObjectsCount; ++i)
					{
						GameObject* gameObject = networkGameObjects[i];

						// TODO(you): World state replication lab session
						if (gameObject->networkId == proxy->gameObject->networkId) continue;
						proxy->replicationServer.create(gameObject->networkId);

					}
				}

				LOG("Message received: hello - from player %s", proxy->name.c_str());
			}
			else
			{
				OutputMemoryStream unwelcomePacket;
				unwelcomePacket << PROTOCOL_ID;
				unwelcomePacket << ServerMessage::Unwelcome;
				sendPacket(unwelcomePacket, fromAddress);
				// NOTE(jesus): Server is full...
				WLOG("Message received: UNWELCOMED hello - server is full");
			}
		}
		else if (message == ClientMessage::Input && App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running)
		{
			// Process the input packet and update the corresponding game object
			if (proxy != nullptr && IsValid(proxy->gameObject))
			{
				// TODO(you): Input Part | Reliability on top of UDP lab session

				uint32 lastInput = 0;
				// Read input data
				while (packet.RemainingByteCount() > 0)
				{
					InputPacketData inputData;
					packet >> inputData.sequenceNumber;
					packet >> inputData.horizontalAxis;
					packet >> inputData.verticalAxis;
					packet >> inputData.buttonBits;

					if (inputData.sequenceNumber >= proxy->nextExpectedInputSequenceNumber)
					{
						proxy->gamepad.horizontalAxis = inputData.horizontalAxis;
						proxy->gamepad.verticalAxis = inputData.verticalAxis;
						unpackInputControllerButtons(inputData.buttonBits, proxy->gamepad);
						proxy->gameObject->behaviour->onInput(proxy->gamepad);
						proxy->nextExpectedInputSequenceNumber = inputData.sequenceNumber + 1;
						lastInput = inputData.sequenceNumber;
					}
				}

				OutputMemoryStream lastInputPacket;
				lastInputPacket << PROTOCOL_ID;
				lastInputPacket << ServerMessage::InputConfirmation;
				lastInputPacket << lastInput;
				sendPacket(lastInputPacket, fromAddress);
			}
		}
		else if (message == ClientMessage::Ping) { 
			// TODO(you): UDP virtual connection lab session
			if (proxy != nullptr) {
				//Update last ping recived from client
				proxy->secondsSinceLastClientPing = 0.0f;
			}
		}
		else if (message == ClientMessage::DeliveryConfirmation) {
			proxy->deliverManager.processAckdSequenceNumbers(packet, &proxy->replicationServer);
		}
	}
}

void ModuleNetworkingServer::onUpdate()
{
	if (state == ServerState::Listening)
	{
		// Handle networked game object destructions
		for (DelayedDestroyEntry &destroyEntry : netGameObjectsToDestroyWithDelay)
		{
			if (destroyEntry.object != nullptr)
			{
				destroyEntry.delaySeconds -= Time.deltaTime;
				if (destroyEntry.delaySeconds <= 0.0f)
				{
					destroyNetworkObject(destroyEntry.object);
					destroyEntry.object = nullptr;
				}
			}
		}

		bool sendPing = false;

		// TODO(you): UDP virtual connection lab session
		// Suma el temps del ultim ping enviat, si es supera (sendPing = true)
		// PING_INTERVAL_SECONDS
		
		secondsSinceLastPingDelivered += Time.deltaTime;
		
		if (secondsSinceLastPingDelivered > PING_INTERVAL_SECONDS)
		{
			secondsSinceLastPingDelivered = 0.0f;

			sendPing = true;
		}

		OutputMemoryStream sbPacket;
		bool sendScoreBoard = false;
		secondsToSendScoreBoard += Time.deltaTime;
		if (secondsToSendScoreBoard > SEND_SCOREBOARD)
		{
			secondsToSendScoreBoard = 0.0f;
			sendScoreBoard = true;

			sbPacket << PROTOCOL_ID;
			sbPacket << ServerMessage::UpdateScoreBoard;
			App->modScreen->screenGame->writeScoresPacket(sbPacket);
		}

		for (ClientProxy &clientProxy : clientProxies)
		{
			if (clientProxy.connected)
			{
				// TODO(you): UDP virtual connection lab session
				// Suma el temps de last packet recieved, si supera el timeout desconecta el client.
				// DISCONNECT_TIMEOUT_SECONDS
				clientProxy.secondsSinceLastClientPing += Time.deltaTime;

				if (clientProxy.secondsSinceLastClientPing > DISCONNECT_TIMEOUT_SECONDS)
				{
					clientProxy.secondsSinceLastClientPing = 0.0f;

					clientProxy.connected = false;
				}

				//Envia un ping.
				else if(sendPing){
					OutputMemoryStream PingPacket;
					PingPacket << PROTOCOL_ID;
					PingPacket << ServerMessage::Ping;
					sendPacket(PingPacket, clientProxy.address);

					WLOG("Ping sent to client proxies.");
				}

				//"si un client da timeout, informar als altres clients de la seva desconexió" futures labs

				// Don't let the client proxy point to a destroyed game object
				if (!IsValid(clientProxy.gameObject))
				{
					clientProxy.gameObject = nullptr;
				}

				if (clientProxy.gameObject == nullptr && App->modScreen->screenGame->GetState() == ScreenGame::MatchState::Running) {
					clientProxy.respawn += Time.deltaTime;

					if (clientProxy.respawn > RESPAWN_PLAYER) {
						clientProxy.respawn = 0.0f;

						// Create new network object
						vec2 initialPosition = 500.0f * vec2{ Random.next() - 0.5f, Random.next() - 0.5f };
						float initialAngle = 360.0f * Random.next();
						clientProxy.gameObject = spawnPlayer(clientProxy.spaceShipType, initialPosition, initialAngle);

						// SendNetworkID to player
						OutputMemoryStream netIDPacket;
						netIDPacket << PROTOCOL_ID;
						netIDPacket << ServerMessage::YourNetID;
						netIDPacket << clientProxy.gameObject->networkId;
						sendPacket(netIDPacket, clientProxy.address);

						// Send all network objects to the new player
						uint16 networkGameObjectsCount;
						GameObject* networkGameObjects[MAX_NETWORK_OBJECTS];
						App->modLinkingContext->getNetworkGameObjects(networkGameObjects, &networkGameObjectsCount);
						for (uint16 i = 0; i < networkGameObjectsCount; ++i)
						{
							GameObject* gameObject = networkGameObjects[i];

							// TODO(you): World state replication lab session
							if (gameObject->networkId == clientProxy.gameObject->networkId) continue;
							clientProxy.replicationServer.create(gameObject->networkId);
						}
					}
				}

				if (sendScoreBoard) {
					OutputMemoryStream finalSBPacket;
					finalSBPacket.Write(sbPacket.GetBufferPtr(), sbPacket.GetSize());
					bool isRespawning = clientProxy.respawn > 0.0f;
					finalSBPacket << isRespawning;
					finalSBPacket << clientProxy.respawn;
					sendPacket(finalSBPacket, clientProxy.address);
				}

				clientProxy.deliverManager.processTiemdOutPackets(&clientProxy.replicationServer);
				clientProxy.replicationServer.processActions(&clientProxy.deliverManager);
				// TODO(you): Reliability on top of UDP lab session
				if (clientProxy.deliverManager.hasSequenceNumberPending()) {
					OutputMemoryStream worldDeliveryPacket;
					worldDeliveryPacket << PROTOCOL_ID;
					worldDeliveryPacket << ServerMessage::WorldDelivery;
					clientProxy.deliverManager.writeAllSequenceNumber(worldDeliveryPacket);
					clientProxy.replicationServer.write(worldDeliveryPacket);
					// TODO(you): World state replication lab session
					sendPacket(worldDeliveryPacket, clientProxy.address);
				}
			}
		}
	}
}

void ModuleNetworkingServer::onConnectionReset(const sockaddr_in & fromAddress)
{
	// Find the client proxy
	ClientProxy *proxy = getClientProxy(fromAddress);

	if (proxy)
	{
		// Clear the client proxy
		proxy->deliverManager.clear();
		proxy->replicationServer.clear();
		destroyClientProxy(proxy);
	}
}

void ModuleNetworkingServer::onDisconnect()
{
	uint16 netGameObjectsCount;
	GameObject *netGameObjects[MAX_NETWORK_OBJECTS];
	App->modLinkingContext->getNetworkGameObjects(netGameObjects, &netGameObjectsCount);
	for (uint32 i = 0; i < netGameObjectsCount; ++i)
	{
		NetworkDestroy(netGameObjects[i]);
	}

	for (ClientProxy &clientProxy : clientProxies)
	{
		clientProxy.deliverManager.clear();
		clientProxy.replicationServer.clear();
		destroyClientProxy(&clientProxy);
	}
	
	nextClientId = 0;

	state = ServerState::Stopped;
}



//////////////////////////////////////////////////////////////////////
// Client proxies
//////////////////////////////////////////////////////////////////////

ModuleNetworkingServer::ClientProxy * ModuleNetworkingServer::createClientProxy()
{
	// If it does not exist, pick an empty entry
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!clientProxies[i].connected)
		{
			return &clientProxies[i];
		}
	}

	return nullptr;
}

ModuleNetworkingServer::ClientProxy * ModuleNetworkingServer::getClientProxy(const sockaddr_in &clientAddress)
{
	// Try to find the client
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clientProxies[i].address.sin_addr.S_un.S_addr == clientAddress.sin_addr.S_un.S_addr &&
			clientProxies[i].address.sin_port == clientAddress.sin_port)
		{
			return &clientProxies[i];
		}
	}

	return nullptr;
}

void ModuleNetworkingServer::destroyClientProxy(ClientProxy *clientProxy)
{
	// Destroy the object from all clients
	if (IsValid(clientProxy->gameObject))
	{
		destroyNetworkObject(clientProxy->gameObject);
	}

    *clientProxy = {};
}


//////////////////////////////////////////////////////////////////////
// Spawning
//////////////////////////////////////////////////////////////////////

GameObject * ModuleNetworkingServer::spawnPlayer(uint8 spaceshipType, vec2 initialPosition, float initialAngle)
{
	// Create a new game object with the player properties
	GameObject *gameObject = NetworkInstantiate();
	gameObject->position = initialPosition;
	gameObject->size = { 100, 100 };
	gameObject->angle = initialAngle;

	// Create sprite
	gameObject->sprite = App->modRender->addSprite(gameObject);
	gameObject->sprite->order = 5;
	if (spaceshipType == 0) {
		gameObject->sprite->texture = App->modResources->spacecraft1;
	}
	else if (spaceshipType == 1) {
		gameObject->sprite->texture = App->modResources->spacecraft2;
	}
	else {
		gameObject->sprite->texture = App->modResources->spacecraft3;
	}

	// Create collider
	gameObject->collider = App->modCollision->addCollider(ColliderType::Player, gameObject);
	gameObject->collider->isTrigger = true; // NOTE(jesus): This object will receive onCollisionTriggered events

	// Create behaviour
	Spaceship * spaceshipBehaviour = App->modBehaviour->addSpaceship(gameObject);
	gameObject->behaviour = spaceshipBehaviour;
	gameObject->behaviour->isServer = true;

	return gameObject;
}


//////////////////////////////////////////////////////////////////////
// Update / destruction
//////////////////////////////////////////////////////////////////////

GameObject * ModuleNetworkingServer::instantiateNetworkObject()
{
	// Create an object into the server
	GameObject * gameObject = Instantiate();

	// Register the object into the linking context
	App->modLinkingContext->registerNetworkGameObject(gameObject);

	// Notify all client proxies' replication manager to create the object remotely
	uint32 netID = gameObject->networkId;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clientProxies[i].connected)
		{
			// TODO(you): World state replication lab session
			clientProxies[i].replicationServer.create(netID);
		}
	}

	return gameObject;
}

void ModuleNetworkingServer::updateNetworkObject(GameObject * gameObject)
{
	// Notify all client proxies' replication manager to destroy the object remotely
	uint32 netID = gameObject->networkId;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clientProxies[i].connected)
		{
			// TODO(you): World state replication lab session
			clientProxies[i].replicationServer.update(netID);
		}
	}
}

void ModuleNetworkingServer::destroyNetworkObject(GameObject * gameObject)
{
	// Notify all client proxies' replication manager to destroy the object remotely
	uint32 netID = gameObject->networkId;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clientProxies[i].connected)
		{
			// TODO(you): World state replication lab session
			clientProxies[i].replicationServer.destroy(netID);
		}
	}

	// Assuming the message was received, unregister the network identity
	App->modLinkingContext->unregisterNetworkGameObject(gameObject);

	// Finally, destroy the object from the server
	Destroy(gameObject);
}

void ModuleNetworkingServer::destroyNetworkObject(GameObject * gameObject, float delaySeconds)
{
	uint32 emptyIndex = MAX_GAME_OBJECTS;
	for (uint32 i = 0; i < MAX_GAME_OBJECTS; ++i)
	{
		if (netGameObjectsToDestroyWithDelay[i].object == gameObject)
		{
			float currentDelaySeconds = netGameObjectsToDestroyWithDelay[i].delaySeconds;
			netGameObjectsToDestroyWithDelay[i].delaySeconds = min(currentDelaySeconds, delaySeconds);
			return;
		}
		else if (netGameObjectsToDestroyWithDelay[i].object == nullptr)
		{
			if (emptyIndex == MAX_GAME_OBJECTS)
			{
				emptyIndex = i;
			}
		}
	}

	ASSERT(emptyIndex < MAX_GAME_OBJECTS);

	netGameObjectsToDestroyWithDelay[emptyIndex].object = gameObject;
	netGameObjectsToDestroyWithDelay[emptyIndex].delaySeconds = delaySeconds;
}


//////////////////////////////////////////////////////////////////////
// Global create / update / destruction of network game objects
//////////////////////////////////////////////////////////////////////

GameObject * NetworkInstantiate()
{
	ASSERT(App->modNetServer->isConnected());

	return App->modNetServer->instantiateNetworkObject();
}

void NetworkUpdate(GameObject * gameObject)
{
	ASSERT(App->modNetServer->isConnected());
	ASSERT(gameObject->networkId != 0);

	App->modNetServer->updateNetworkObject(gameObject);
}

void NetworkDestroy(GameObject * gameObject)
{
	NetworkDestroy(gameObject, 0.0f);
}

void NetworkDestroy(GameObject * gameObject, float delaySeconds)
{
	ASSERT(App->modNetServer->isConnected());
	ASSERT(gameObject->networkId != 0);

	App->modNetServer->destroyNetworkObject(gameObject, delaySeconds);
}
