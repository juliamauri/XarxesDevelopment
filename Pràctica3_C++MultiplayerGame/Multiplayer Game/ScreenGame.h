#pragma once

#define TIME_NEXT_MATCH 20.f
#define TIME_STARTING_MATCH 10.f
#define TIME_MATCH 120.f

class ScreenGame : public Screen
{
public:
	enum MatchState {
		Waiting,
		Running,
		End
	};

	bool isServer = true;
	int serverPort;
	const char *serverAddress = "127.0.0.1";
	const char *playerName = "player";
	uint8 spaceshipType = 0;

	void AddPlayer(const char* name, uint8 spaceType);
	MatchState GetState()const;

	void writeScoresPacket(OutputMemoryStream& packet);
	void onPacketRecieved(const InputMemoryStream& packet);
private:

	struct ScoreBoard {
		std::vector<std::tuple<std::string, uint8, uint32>> scores;
		float timeRemaining = TIME_STARTING_MATCH;
		MatchState mState = Waiting;
	};
	ScoreBoard currentScoreBoard;

	bool respawn = false;
	float timeToRespawn = 0.0f;

	void enable() override;

	void update() override;

	void gui() override;

	void disable() override;
};

