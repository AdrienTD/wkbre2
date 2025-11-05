#pragma once

class Server;

class QuickSkirmishMenu
{
public:
	QuickSkirmishMenu() = default;

	void imguiWindow();
	void applySettings(Server& server);

private:
	enum GameMode {
		DevMode,
		StandardMode,
		ValhallaMode
	};
	GameMode mode = DevMode;

	int startFood = 2000;
	int startWood = 2000;
	int startGold = 500;
	int populationLimit = 800;
	int randomiseResources = 0;
	int loseOnManorDestroyed = 1;
	bool kingPiece = false;
	bool clearFogOfWar = false;
	bool allowDiplomacy = false;
	bool teamReconnaissance = false;
	bool barbarianTribes = false;
	bool campaignMode = false;

	int resourcePoints = 2500;
	int victoryPointLimit = 10000;
	int valhallaModeFood = 20000;
	int unitLives = 0;
	int capturableFlags = 4;
};