// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "GameTextWindow.h"
#include "util/GSFileParser.h"
#include "gameset.h"

void GameTextWindow::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "PAGE") {
			pages.emplace_back();
			pages.back().parse(gsf, gs);
		}
		else if (tag == "BUTTON") {
			buttons.emplace_back();
			buttons.back().parse(gsf, gs);
		}
		else if (tag == "GAME_TEXT_WINDOW_END")
			break;
		gsf.advanceLine();
	}
}

void GameTextWindow::Page::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "TEXT_BODY")
			textBody = gsf.nextString();
		else if (tag == "ACTIVATION_SOUND")
			activationSound = gsf.nextString(true);
		else if (tag == "PAGE_END")
			break;
		gsf.advanceLine();
	}
}

void GameTextWindow::Button::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "BUTTON_TEXT") {
			text = gsf.nextString();
		}
		else if (tag == "ON_CLICK_WINDOW_ACTION") {
			onClickWindowAction = Tags::GTW_BUTTON_WINDOW_ACTION_tagDict.getTagID(gsf.nextString().c_str());
		}
		else if (tag == "ON_CLICK_SEQUENCE") {
			onClickSequence.init(gsf, gs, "END_ON_CLICK_SEQUENCE");
		}
		else if (tag == "BUTTON_END")
			break;
		gsf.advanceLine();
	}
}
