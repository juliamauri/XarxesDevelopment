#pragma once

#include "ModuleNetworking.h"

class ModuleNetworkingServer : public ModuleNetworking
{
public:

	//////////////////////////////////////////////////////////////////////
	// ModuleNetworkingServer public methods
	//////////////////////////////////////////////////////////////////////

	void setListenPort(int port);

	void RespawnPlayers();

	void AddPoint(GameObject* toAdd);

private:

	//////////////////////////////////////////////////////////////////////
	// ModuleNetworking virtual methods
	//////////////////////////////////////////////////////////////////////

	bool isServer() const override { return true; }

	void onStart() override;

	void onGui() override;

	void onPacketReceived(const InputMemoryStream &packet, const sockaddr_in &fromAddress) override;

	void onUpdate() override;

	void onConnectionReset(const sockaddr_in &fromAddress) override;

	void onDisconnect() override;

	//////////////////////////////////////////////////////////////////////
	// Client proxies
	//////////////////////////////////////////////////////////////////////

	uint32 nextClientId = 0;

	struct ClientProxy
	{
		bool connected = false;
		sockaddr_in address;
		uint32 clientId;
		uint32 scoreBoardID;
		uint8 spaceShipType;
		std::string name;
		GameObject *gameObject = nullptr;
		float respawn = 0.0f;

		// TODO(you): UDP virtual connection lab session
		float secondsSinceLastClientPing = 0.0f;
		// TODO(you): World state replication lab session
		ReplicationManagerServer replicationServer;
		// TODO(you): Reliability on top of UDP lab session
		DeliveryManager deliverManager;

		uint32 nextExpectedInputSequenceNumber = 0;
		InputController gamepad;
	};

	ClientProxy clientProxies[MAX_CLIENTS];

	ClientProxy * createClientProxy();

	ClientProxy * getClientProxy(const sockaddr_in &clientAddress);

    void destroyClientProxy(ClientProxy *clientProxy, bool fromDisconnect = false);



public:

	//////////////////////////////////////////////////////////////////////
	// Spawning network objects
	//////////////////////////////////////////////////////////////////////

	GameObject * spawnPlayer(uint8 spaceshipType, vec2 initialPosition, float initialAngle);



private:

	//////////////////////////////////////////////////////////////////////
	// Updating / destroying network objects
	//////////////////////////////////////////////////////////////////////

	GameObject * instantiateNetworkObject();
	friend GameObject *(NetworkInstantiate)();

	void updateNetworkObject(GameObject *gameObject);
	friend void (NetworkUpdate)(GameObject *);

	void destroyNetworkObject(GameObject *gameObject);
	void destroyNetworkObject(GameObject *gameObject, float delaySeconds);
	friend void (NetworkDestroy)(GameObject *);
	friend void (NetworkDestroy)(GameObject *, float delaySeconds);

	struct DelayedDestroyEntry
	{
		float delaySeconds = 0.0f;
		GameObject *object = nullptr;
	};

	DelayedDestroyEntry netGameObjectsToDestroyWithDelay[MAX_GAME_OBJECTS] = {};



	//////////////////////////////////////////////////////////////////////
	// State
	//////////////////////////////////////////////////////////////////////

	enum class ServerState
	{
		Stopped,
		Listening
	};

	ServerState state = ServerState::Stopped;

	uint16 listenPort = 0;

	// TODO(you): UDP virtual connection lab session
	float secondsSinceLastPingDelivered = 0.0f;

	float secondsToSendScoreBoard = 0.0f;
};


// NOTE(jesus): It creates a game object into the network. Use
// this method instead of Instantiate() to create network objects.
// It makes sure the object creation is replicated over the network.
GameObject * NetworkInstantiate();

// NOTE(jesus): It marks an object for replication update.
void NetworkUpdate(GameObject *gameObject);

// NOTE(jesus): For network objects, use this version instead of
// the default Destroy(GameObject *gameObject) one. This one makes
// sure to notify the destruction of the object to all connected
// machines.
void NetworkDestroy(GameObject *gameObject);
void NetworkDestroy(GameObject *gameObject, float delaySeconds);
