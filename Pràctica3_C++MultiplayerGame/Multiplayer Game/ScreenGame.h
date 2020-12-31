#pragma once

#define TIME_NEXT_MATCH 20.f
#define TIME_STARTING_MATCH 10.f
#define TIME_MATCH 120.f

class ScreenGame : public Screen
{
public:

	bool isServer = true;
	int serverPort;
	const char *serverAddress = "127.0.0.1";
	const char *playerName = "player";
	uint8 spaceshipType = 0;

	void onPacketRecieved(InputMemoryStream& packet);

private:

	enum MatchState {
		Waiting,
		Running,
		End
	};

	struct ScoreBoard {
		std::vector<std::pair<std::string, uint32>> scores;
		float timeRemaining = TIME_MATCH;
		MatchState mState = Waiting;
	};

	ScoreBoard currentScoreBoard;

	void enable() override;

	void update() override;

	void gui() override;

	void disable() override;
};

