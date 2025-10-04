// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct GameSet;

struct GameSetDebugger {
	const GameSet* gameSet = nullptr;
	GameSetDebugger() {}
	GameSetDebugger(const GameSet* gameSet) : gameSet(gameSet) {}
	void setGameSet(const GameSet* newGameSet) { gameSet = newGameSet; }
	void draw();
};