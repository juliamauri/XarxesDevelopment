
#include "Networks.h"
#include "ScreenGame.h"

GameObject *spaceTopLeft = nullptr;
GameObject *spaceTopRight = nullptr;
GameObject *spaceBottomLeft = nullptr;
GameObject *spaceBottomRight = nullptr;

void ScreenGame::onPacketRecieved(InputMemoryStream& packet)
{
	uint32 totalScores;
	packet >> totalScores;

	for (uint32 i = 0; i < totalScores; i++)
	{
		std::string playerName;
		packet >> playerName;
		uint32 score;
		packet >> score;
		currentScoreBoard.scores.push_back({ playerName, score });
	}

	packet >> currentScoreBoard.timeRemaining;
	packet >> currentScoreBoard.mState;
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
	}
}

void ScreenGame::gui()
{
	if (!isServer)
	{
		bool scoreBoardActive = currentScoreBoard.mState != End;
		if (ImGui::Begin("ScoreBoard", &scoreBoardActive))
		{
			//Time Remaining
			ImGui::Text("Time remaining: %i seconds", currentScoreBoard.timeRemaining);
			ImGui::Text("");
			ImGui::Separator();
			ImGui::Text("");
			//Kill Score
			for (int i = 0; i < currentScoreBoard.scores.size(); i++)
			{
				ImGui::Text("%s  -  %s  -  %i", std::get<0>(currentScoreBoard.scores[i]), std::get<1>(currentScoreBoard.scores[i]), std::get<2>(currentScoreBoard.scores[i]));
			}
		}
		ImGui::End();

		bool finalScoreBoard = currentScoreBoard.mState == End;
		if (ImGui::Begin("Final Score", &finalScoreBoard))
		{
			//Time Remaining to next match
			ImGui::Text("Time remaining: %i seconds", currentScoreBoard.timeRemaining);
			ImGui::Text("");
			ImGui::Separator();
			ImGui::Text("");
			//Total Kill Score
			for (int i = 0; i < currentScoreBoard.scores.size(); i++)
			{
				ImGui::Text("%s  -  %s  -  %i", std::get<0>(currentScoreBoard.scores[i]), std::get<1>(currentScoreBoard.scores[i]), std::get<2>(currentScoreBoard.scores[i]));
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
