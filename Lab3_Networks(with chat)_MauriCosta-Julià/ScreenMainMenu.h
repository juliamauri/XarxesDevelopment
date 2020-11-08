#pragma once

class ScreenMainMenu : public Screen
{
public:
	bool disconnectedFromServer = false;

private:

	void enable() override;

	void gui() override;
};
