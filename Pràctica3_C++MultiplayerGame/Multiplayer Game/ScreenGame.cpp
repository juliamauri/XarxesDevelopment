
#include "Networks.h"
#include "ScreenGame.h"

GameObject *spaceTopLeft = nullptr;
GameObject *spaceTopRight = nullptr;
GameObject *spaceBottomLeft = nullptr;
GameObject *spaceBottomRight = nullptr;

uint32 ScreenGame::AddPlayer(const char* name, uint8 spaceType)
{
	static uint32 infinite_id = 0;
	currentScoreBoard.scores.insert({ infinite_id++, std::make_tuple(name, spaceType, 0) });
	return infinite_id - 1;
}

void ScreenGame::DeletePlayer(uint32 id)
{
	currentScoreBoard.scores.erase(id);
}

ScreenGame::MatchState ScreenGame::GetState() const
{
	return currentScoreBoard.mState;
}

void ScreenGame::ScorePoint(uint32 id)
{
	std::tuple<std::string, uint8, uint32>& t = currentScoreBoard.scores.at(id);
	uint32 currentScore = std::get<2>(t);
	std::get<2>(t) = currentScore + 1;
}

void ScreenGame::EndMatch()
{
	if (currentScoreBoard.mState == ScreenGame::MatchState::Running)
		currentScoreBoard.timeRemaining = 0.0f;
}

void ScreenGame::writeScoresPacket(OutputMemoryStream& packet)
{
	size_t totalScores = currentScoreBoard.scores.size();
	packet << totalScores;

	for (auto score : currentScoreBoard.scores)
	{
		std::string playerName = std::get<0>(score.second);
		packet << playerName;
		uint8 spaceType = std::get<1>(score.second);
		packet << spaceType;
		uint32 sN = std::get<2>(score.second);
		packet << sN;
		uint32 uniqueID = score.first;
		packet << uniqueID;
	}

	packet << currentScoreBoard.timeRemaining;
	packet << currentScoreBoard.mState;
}

void ScreenGame::onPacketRecieved(const InputMemoryStream& packet)
{
	size_t totalScores;
	packet >> totalScores;

	currentScoreBoard.scores.clear();

	for (size_t i = 0; i < totalScores; i++)
	{
		std::string playerName;
		packet >> playerName;
		uint8 spaceType;
		packet >> spaceType;
		uint32 score;
		packet >> score;
		uint32 uniqueID;
		packet >> uniqueID;
		currentScoreBoard.scores.insert({ uniqueID, std::make_tuple(playerName, spaceType, score) });
	}

	packet >> currentScoreBoard.timeRemaining;
	MatchState lastState = currentScoreBoard.mState;
	packet >> currentScoreBoard.mState;

	if (lastState == MatchState::Running && currentScoreBoard.mState == MatchState::End) {
		maximumPoints = 0;
		winnersID.clear();
		for (auto score : currentScoreBoard.scores)
		{
			uint32 sN = std::get<2>(score.second);
			if (sN > maximumPoints) {
				maximumPoints = sN;
				winnersID.clear();
				winnersID.push_back(std::get<0>(score.second));
			}
			else if (sN == maximumPoints)
				winnersID.push_back(std::get<0>(score.second));
		}
	}

	packet >> respawn;
	packet >> timeToRespawn;
}

void ScreenGame::clear()
{
	currentScoreBoard.scores.clear();
	currentScoreBoard.mState = Waiting;
	currentScoreBoard.timeRemaining = TIME_STARTING_MATCH;
	respawn = false;
	timeToRespawn = 0.0f;
	winnersID.clear();
	maximumPoints = 0;
}

void ScreenGame::enable()
{
	if (isServer)
	{
		App->modNetServer->setListenPort(serverPort);
		App->modNetServer->setEnabled(true);
	}
	else
	{
		App->modNetClient->setServerAddress(serverAddress, serverPort);
		App->modNetClient->setPlayerInfo(playerName, spaceshipType);
		App->modNetClient->setEnabled(true);
	}

	spaceTopLeft = Instantiate();
	spaceTopLeft->sprite = App->modRender->addSprite(spaceTopLeft);
	spaceTopLeft->sprite->texture = App->modResources->space;
	spaceTopLeft->sprite->order = -1;
	spaceTopRight = Instantiate();
	spaceTopRight->sprite = App->modRender->addSprite(spaceTopRight);
	spaceTopRight->sprite->texture = App->modResources->space;
	spaceTopRight->sprite->order = -1;
	spaceBottomLeft = Instantiate();
	spaceBottomLeft->sprite = App->modRender->addSprite(spaceBottomLeft);
	spaceBottomLeft->sprite->texture = App->modResources->space;
	spaceBottomLeft->sprite->order = -1;
	spaceBottomRight = Instantiate();
	spaceBottomRight->sprite = App->modRender->addSprite(spaceBottomRight);
	spaceBottomRight->sprite->texture = App->modResources->space;
	spaceBottomRight->sprite->order = -1;
}

void ScreenGame::update()
{
	if (!(App->modNetServer->isConnected() || App->modNetClient->isConnected()))
	{
		App->modScreen->swapScreensWithTransition(this, App->modScreen->screenMainMenu);
	}
	else
	{
		if (!isServer)
		{
			vec2 camPos = App->modRender->cameraPosition;
			vec2 bgSize = spaceTopLeft->sprite->texture->size;
			spaceTopLeft->position = bgSize * floor(camPos / bgSize);
			spaceTopRight->position = bgSize * (floor(camPos / bgSize) + vec2{ 1.0f, 0.0f });
			spaceBottomLeft->position = bgSize * (floor(camPos / bgSize) + vec2{ 0.0f, 1.0f });
			spaceBottomRight->position = bgSize * (floor(camPos / bgSize) + vec2{ 1.0f, 1.0f });;
		}
		else {
			currentScoreBoard.timeRemaining -= Time.deltaTime;
			if (currentScoreBoard.timeRemaining < 0.f) {
				switch (currentScoreBoard.mState)
				{
				case ScreenGame::MatchState::Waiting:
					currentScoreBoard.timeRemaining = TIME_MATCH;
					currentScoreBoard.mState = ScreenGame::MatchState::Running;
					//Spawn all Players
					App->modNetServer->RespawnPlayers();
					break;
				case ScreenGame::MatchState::Running:
					//Notify To Stop
					currentScoreBoard.timeRemaining = TIME_NEXT_MATCH;
					currentScoreBoard.mState = ScreenGame::MatchState::End;
					break;
				case ScreenGame::MatchState::End:
					//Reset ScoreBoard
					currentScoreBoard.timeRemaining = TIME_STARTING_MATCH;
					std::map<uint32, std::tuple<std::string, uint8, uint32>> resetScores;
					for (auto score : currentScoreBoard.scores)
					{
						std::string playerName = std::get<0>(score.second);
						uint8 spaceType = std::get<1>(score.second);
						uint32 SN = std::get<2>(score.second);
						resetScores.insert({ score.first, std::make_tuple(playerName, spaceType, 0) });
					}
					currentScoreBoard.scores = resetScores;
					currentScoreBoard.mState = ScreenGame::MatchState::Waiting;
					break;
				}
			}
		}
	}
}

void ScreenGame::gui()
{
	if (!isServer)
	{
		
		if (ImGui::Begin("ScoreBoard"))
		{
			//Time Remaining
			switch (currentScoreBoard.mState)
			{
			case MatchState::Waiting:
				ImGui::Text("Match starting in %.f", currentScoreBoard.timeRemaining);
				break;
			case MatchState::Running:
				ImGui::Text("Time remaining: %.f seconds", currentScoreBoard.timeRemaining);
				break;
			case MatchState::End:
				ImGui::Text("Next match starting in %.f seconds", currentScoreBoard.timeRemaining);
				break;
			}

			if (respawn)
			{
				ImGui::Text("");
				ImGui::Separator();
				ImGui::Text("You were killed. Respawning in %.f", RESPAWN_PLAYER - timeToRespawn);
			}

			if (currentScoreBoard.mState == End)
			{
				if (winnersID.size() == 1)
				{
					ImGui::Text("");
					ImGui::Separator();
					ImGui::Text("The winner is %s with %u kills", winnersID[0].c_str(), maximumPoints);
				}
				else
				{
					ImGui::Text("");
					ImGui::Separator();
					ImGui::Text("The winners are\n");
					for (auto winner : winnersID)
					{
						ImGui::Text("%s\n", winner.c_str());
					}
					ImGui::Text("with %u kills", maximumPoints);
				}
			}
			
			ImGui::Text("");
			ImGui::Separator();
			ImGui::Text("");

			//Kill Score
			for (auto score : currentScoreBoard.scores)
			{
				ImGui::Text("%s  -  %u", std::get<0>(score.second).c_str(), std::get<2>(score.second));
			}
		}
		ImGui::End();
	}
}

void ScreenGame::disable()
{
	Destroy(spaceTopLeft);
	Destroy(spaceTopRight);
	Destroy(spaceBottomLeft);
	Destroy(spaceBottomRight);
}
