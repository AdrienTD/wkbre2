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
		ValhallaMode,
		CampaignMode
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

	int resourcePoints = 2500;
	int victoryPointLimit = 10000;
	int valhallaModeFood = 20000;
	int unitLives = 0;
	int capturableFlags = 4;

	int originalCampaignCelestialPath = 0;
	int battlesCampaignImperialTechLevel = 0;
	int battlesCampaignPaganTechLevel = 0;
	int battlesCampaignRenaissanceTechLevel = 0;
};