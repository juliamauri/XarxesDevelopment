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
			bool existsOrDeleted = (App->modLinkingContext->getNetworkGameObject(netID));
			if (!existsOrDeleted) {
				for (auto netGO : netGODelted)
					if (existsOrDeleted = (netID == netGO))break;
			}
	
			uint32 goType;
			packet >> goType;

			if (existsOrDeleted)
			{
				GameObject dummy;

				packet >> dummy.tag;
				dummy.DeSerialize(packet);
				LOG("Gameobject exists or was previously deleted, filling dummy");
				continue;
			}

			GameObject* go = ModuleGameObject::Instantiate();
			if(goType < 3){
				// Create sprite
				go->sprite = App->modRender->addSprite(go);
				go->sprite->order = 5;
				if (goType == 0) {
					go->sprite->texture = App->modResources->spacecraft1;
				}
				else if (goType == 1) {
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
			else if (goType == 3) {
				go->sprite = App->modRender->addSprite(go);
				go->sprite->order = 3;
				go->sprite->texture = App->modResources->laser;

				Laser* laserBehaviour = App->modBehaviour->addLaser(go);
				laserBehaviour->isServer = false;
			}
			else if(goType == 4){
				go->sprite = App->modRender->addSprite(go);
				go->sprite->texture = App->modResources->explosion1;
				go->sprite->order = 100;

				go->animation = App->modRender->addAnimation(go);
				go->animation->clip = App->modResources->explosionClip;

				App->modSound->playAudioClip(App->modResources->audioClipExplosion);
			}
			
			App->modLinkingContext->registerNetworkGameObjectWithNetworkId(go, netID);
			packet >> go->tag;
			go->DeSerialize(packet);
			LOG("Creating gameobject %i, %i", goType, netID);
			break;
		}
		case ReplicationAction::Update:
		{
			GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
			if (go == nullptr)
			{
				GameObject dummy;

				dummy.DeSerialize(packet);

				bool isSpaceCraft;
				packet >> isSpaceCraft;
				if (isSpaceCraft) {
					uint8 hitpoints;
					packet >> hitpoints;
				}
				continue;
			}
			go->DeSerialize(packet);
			go->state = GameObject::State::UPDATING;

			bool isSpaceCraft;
			packet >> isSpaceCraft;
			if (isSpaceCraft) {
				Spaceship* spaceCraft = dynamic_cast<Spaceship*>(go->behaviour);
				packet >> spaceCraft->hitPoints;
			}
			break;
		}
		case ReplicationAction::Destroy:
		{
			GameObject* go = App->modLinkingContext->getNetworkGameObject(netID);
			if (go)
			{
				netGODelted.push_back(netID);
				App->modLinkingContext->unregisterNetworkGameObject(go);
				ModuleGameObject::Destroy(go);
				LOG("Deleting gameobject %i", netID);
			}
			break;
		}
		}
	}
}

void ReplicationManagerClient::clear()
{
	netGODelted.clear();
}
