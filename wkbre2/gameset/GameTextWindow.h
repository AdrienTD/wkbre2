// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <vector>
#include "actions.h"

struct GameSet;
struct GSFileParser;

struct GameTextWindow {
	struct Page {
		std::string textBody;
		std::string activationSound;
		void parse(GSFileParser& gsf, GameSet& gs);
	};
	struct Button {
		std::string text;
		int onClickWindowAction;
		ActionSequence onClickSequence;
		void parse(GSFileParser& gsf, GameSet& gs);
	};
	std::vector<Page> pages;
	std::vector<Button> buttons;
	void parse(GSFileParser& gsf, GameSet& gs);
};