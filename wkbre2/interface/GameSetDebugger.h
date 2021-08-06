#pragma once

struct GameSet;

struct GameSetDebugger {
	GameSet* gameSet = nullptr;
	GameSetDebugger() {}
	GameSetDebugger(GameSet* gameSet) : gameSet(gameSet) {}
	void setGameSet(GameSet* newGameSet) { gameSet = newGameSet; }
	void draw();
};