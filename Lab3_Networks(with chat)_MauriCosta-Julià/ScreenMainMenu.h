#pragma once

class ScreenMainMenu : public Screen
{
public:
	bool disconnectedFromServer = false;
	bool kickedFromServer = false;

private:

	void enable() override;

	void gui() override;
};
