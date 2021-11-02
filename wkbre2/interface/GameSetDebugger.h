// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct GameSet;

struct GameSetDebugger {
	GameSet* gameSet = nullptr;
	GameSetDebugger() {}
	GameSetDebugger(GameSet* gameSet) : gameSet(gameSet) {}
	void setGameSet(GameSet* newGameSet) { gameSet = newGameSet; }
	void draw();
};