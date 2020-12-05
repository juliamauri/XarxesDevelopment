#include "Networks.h"
#include "ReplicationManagerClient.h"

#include "ModuleGameObject.h"

// TODO(you): World state replication lab session

void ReplicationManagerClient::read(const InputMemoryStream& packet)
{
	while (packet.RemainingByteCount() > 0)
	{
		uint32 netID;
		packet >> netID;

		ReplicationAction action;
		packet >> action;

		switch (action)
		{
		case ReplicationAction::Create:
		{
			uint32 spaceshipType;
			packet >> spaceshipType;

			GameObject* go = ModuleGameObject::Instantiate();
			if(spaceshipType < 3){
				// Create sprite
				go->sprite = App->modRender->addSprite(go);
				go->sprite->order = 5;
				if (spaceshipType == 0) {
					go->sprite->texture = App->modResources->spacecraft1;
				}
				else if (spaceshipType == 1) {
					go->sprite->texture = App->modResources->spacecraft2;
				}
				else {
					go->sprite->texture = App->modResources->spacecraft3;
				}

				// Create collider
				go->collider = App->modCollision->addCollider(ColliderType::Player, go);
				go->collider->isTrigger = true; // NOTE(jesus): This object will receive onCollisionTriggered events

				// Create behaviour
				Spaceship* spaceshipBehaviour = App->modBehaviour->addSpaceship(go);
				go->behaviour = spaceshipBehaviour;
				go->behaviour->isServer = false;
			
			}
			
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
