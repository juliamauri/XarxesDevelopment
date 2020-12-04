#include "Networks.h"
#include "ReplicationManagerClient.h"

#include "ModuleGameObject.h"

// TODO(you): World state replication lab session

void ReplicationManagerClient::read(const InputMemoryStream& packet)
{
	while (packet.GetSize() > 0)
	{
		uint32 netID;
		packet >> netID;

		ReplicationAction action;
		packet >> action;

		switch (action)
		{
		case ReplicationAction::Create:
		{
			GameObject* go = ModuleGameObject::Instantiate();
			App->modLinkingContext->registerNetworkGameObjectWithNetworkId(go, netID);
			go->DeSerialize(packet);
			break;
		}
		case ReplicationAction::Update:
		{
			GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
			go->DeSerialize(packet);
			go->state = GameObject::State::UPDATING;
			break;
		}
		case ReplicationAction::Destroy:
		{
			GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
			App->modLinkingContext->unregisterNetworkGameObject(go);
			ModuleGameObject::Destroy(go);
			break;
		}
		}
	}
}
