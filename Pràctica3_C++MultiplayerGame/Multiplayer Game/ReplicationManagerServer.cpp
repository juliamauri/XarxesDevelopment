#include "Networks.h"
#include "ReplicationManagerServer.h"

// TODO(you): World state replication lab session

void ReplicationManagerServer::create(uint32 networkId)
{
	replicationCommands.insert({ networkId, { ReplicationAction::Create, networkId} });
}

void ReplicationManagerServer::update(uint32 networkId)
{
	replicationCommands.at(networkId).action = ReplicationAction::Update;
}

void ReplicationManagerServer::destroy(uint32 networkId)
{
	replicationCommands.at(networkId).action = ReplicationAction::Destroy;
}

void ReplicationManagerServer::write(OutputMemoryStream& packet)
{
	for (auto command = replicationCommands.begin(); command != replicationCommands.end(); command++ ) {
		
		uint32 netID = (*command).first;
		ReplicationAction action = (*command).second.action;

		switch (action)
		{ 
		case ReplicationAction::Create:
			packet << netID;
			packet << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				go->Serialize(packet);
			}

			(*command).second.action = ReplicationAction::None;
			break;
		case ReplicationAction::Update:
			packet << netID;
			packet << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				go->Serialize(packet);
			}

			(*command).second.action = ReplicationAction::None;
			break;
		case ReplicationAction::Destroy:
			packet << netID;
			packet << action;
			replicationCommands.erase(netID);
			break;
		}
	}
}
