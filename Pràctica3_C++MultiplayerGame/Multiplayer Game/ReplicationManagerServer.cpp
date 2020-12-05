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

bool ReplicationManagerServer::write(OutputMemoryStream& packet)
{
	bool ret = false;
	for (auto command = replicationCommands.begin(); command != replicationCommands.end(); command++ ) {
		ReplicationAction action = (*command).second.action;
		if (action == ReplicationAction::None) continue;

		uint32 netID = (*command).first;

		switch (action)
		{ 
		case ReplicationAction::Create:
			packet << netID;
			packet << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				Texture* tex = go->sprite->texture;
				uint32 goType = 4;
				if (tex == App->modResources->spacecraft1)
					goType = 0;
				else if (tex == App->modResources->spacecraft2)
					goType = 1;
				else if (tex == App->modResources->spacecraft3)
					goType = 2;
				else if (tex == App->modResources->laser) 
					goType = 3;
				else if (tex == App->modResources->explosion1)
					goType = 4;
					
				packet << goType;
				packet << go->tag;

				go->Serialize(packet);
			}

			(*command).second.action = ReplicationAction::None;
			ret = true;
			break;
		case ReplicationAction::Update:
			packet << netID;
			packet << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				go->Serialize(packet);
			}

			(*command).second.action = ReplicationAction::None;
			ret = true;
			break;
		case ReplicationAction::Destroy:
			packet << netID;
			packet << action;
			replicationCommands.erase(netID);
			ret = true;
			break;
		}
	}
	return ret;
}
