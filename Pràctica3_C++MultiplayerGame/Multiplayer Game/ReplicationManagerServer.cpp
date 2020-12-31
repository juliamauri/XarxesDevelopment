#include "Networks.h"
#include "ReplicationManagerServer.h"

#include <stack>

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

void ReplicationManagerServer::processActions(DeliveryManager* delivery)
{
	std::stack<uint32> toDelete;

	for (auto command = replicationCommands.begin(); command != replicationCommands.end(); command++) {
		ReplicationAction action = (*command).second.action;
		if (action == ReplicationAction::None) continue;

		delivery->createDelivery();
		OutputMemoryStream savePacket;
		uint32 netID = (*command).first;

		switch (action)
		{
		case ReplicationAction::Create:
			savePacket << netID;
			savePacket << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				Texture* tex = go->sprite->texture;
				uint32 goType = 5;
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

				savePacket << goType;
				savePacket << go->tag;

				go->Serialize(savePacket);
			}

			(*command).second.action = ReplicationAction::None;
			break;
		case ReplicationAction::Update:
			savePacket << netID;
			savePacket << action;

			{
				GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
				go->Serialize(savePacket);
				Texture* tex = go->sprite->texture;
				bool isSpaceCraft = false;
				if (tex == App->modResources->spacecraft1)
					isSpaceCraft = true;
				else if (tex == App->modResources->spacecraft2)
					isSpaceCraft = true;
				else if (tex == App->modResources->spacecraft3)
					isSpaceCraft = true;

				savePacket << isSpaceCraft;
				if (isSpaceCraft) {
					Spaceship* spaceCraft = dynamic_cast<Spaceship*>(go->behaviour);
					savePacket << spaceCraft->hitPoints;
				}
				
			}

			(*command).second.action = ReplicationAction::None;
			break;
		case ReplicationAction::Destroy:
			savePacket << netID;
			savePacket << action;
			toDelete.push(netID);
			break;
		}

		actionsToWrite.push_back(savePacket);
	}

	while (!toDelete.empty()) {
		replicationCommands.erase(toDelete.top());
		toDelete.pop();
	}
}

void ReplicationManagerServer::write(OutputMemoryStream& packet)
{
	for (auto actionW = actionsToWrite.begin(); actionW != actionsToWrite.end(); actionW++)
		packet.Write(actionW->GetBufferPtr(), actionW->GetSize());
}

void ReplicationManagerServer::popFrontAction()
{
	actionsToWrite.pop_front();
}