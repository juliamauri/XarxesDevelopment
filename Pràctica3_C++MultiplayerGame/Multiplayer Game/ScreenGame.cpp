
#include "Networks.h"
#include "ScreenGame.h"

GameObject *spaceTopLeft = nullptr;
GameObject *spaceTopRight = nullptr;
GameObject *spaceBottomLeft = nullptr;
GameObject *spaceBottomRight = nullptr;

uint32 ScreenGame::AddPlayer(const char* name, uint8 spaceType)
{
	currentScoreBoard.scores.push_back(std::make_tuple(name, spaceType, 0));
	return currentScoreBoard.scores.size() - 1;
}

ScreenGame::MatchState ScreenGame::GetState() const
{
	return currentScoreBoard.mState;
}

void ScreenGame::ScorePoint(uint32 id)
{
	std::tuple<std::string, uint8, uint32>& t = currentScoreBoard.scores[id];
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

	for (uint32 i = 0; i < totalScores; i++)
	{
		std::string playerName = std::get<0>(currentScoreBoard.scores[i]);
		packet << playerName;
		uint8 spaceType = std::get<1>(currentScoreBoard.scores[i]);
		packet << spaceType;
		uint32 score = std::get<2>(currentScoreBoard.scores[i]);
		packet << score;
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
		currentScoreBoard.scores.push_back(std::make_tuple( playerName, spaceType, score ));
	}

	packet >> currentScoreBoard.timeRemaining;
	packet >> currentScoreBoard.mState;

	packet >> respawn;
	packet >> timeToRespawn;
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
					{
						uint32 maximumPoints = 0;
						winnersID.clear();
						for (uint32 i = 0; i < currentScoreBoard.scores.size(); i++)
						{
							uint32 score = std::get<2>(currentScoreBoard.scores[i]);
							if (score > maximumPoints) {
								maximumPoints = score;
								winnersID.clear();
								winnersID.push_back(i);
							}
							else if (score == maximumPoints)
								winnersID.push_back(i);
						}
					}
					break;
				case ScreenGame::MatchState::End:
					//Reset ScoreBoard
					currentScoreBoard.timeRemaining = TIME_STARTING_MATCH;
					std::vector<std::tuple<std::string, uint8, uint32>> resetScores;
					for (uint32 i = 0; i < currentScoreBoard.scores.size(); i++)
					{
						std::string playerName = std::get<0>(currentScoreBoard.scores[i]);
						uint8 spaceType = std::get<1>(currentScoreBoard.scores[i]);
						uint32 score = std::get<2>(currentScoreBoard.scores[i]);
						resetScores.push_back(std::make_tuple(playerName, spaceType, 0));
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
			if (currentScoreBoard.mState == Waiting)
			{
				ImGui::Text("Match starting in %.f", currentScoreBoard.timeRemaining);
			}
			else if(currentScoreBoard.mState == Running)
			{
				ImGui::Text("Time remaining: %.f seconds", currentScoreBoard.timeRemaining);
			}
			else
			{
				ImGui::Text("Next match starting in %.f seconds", currentScoreBoard.timeRemaining);
			}

			if (respawn)
			{
				ImGui::Text("");
				ImGui::Separator();
				ImGui::Text("You were killed. Respawning in %.f", RESPAWN_PLAYER - timeToRespawn);
			}

			if (currentScoreBoard.mState == End)
			{
				ImGui::Text("");
				ImGui::Separator();
				ImGui::Text("The winner is %s with %u kills", std::get<0>(currentScoreBoard.scores[winnerID]).c_str(), std::get<2>(currentScoreBoard.scores[winnerID]));
			}
			
			ImGui::Text("");
			ImGui::Separator();
			ImGui::Text("");

			//Kill Score
			for (int i = 0; i < currentScoreBoard.scores.size(); i++)
			{
				ImGui::Text("%s  -  %u", std::get<0>(currentScoreBoard.scores[i]).c_str(), std::get<2>(currentScoreBoard.scores[i]));
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
