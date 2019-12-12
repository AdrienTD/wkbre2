#include "gameset.h"
#include "../file.h"
#include "../util/GSFileParser.h"
#include "../tags.h"
#include <vector>
#include <string>
#include <set>

void ignoreBlueprint(GSFileParser &gsf, const std::string &tag)
{
	std::string endtag = "END_" + tag;
	gsf.advanceLine();
	while (!gsf.eof)
	{
		if (gsf.nextTag() == endtag)
			break;
		gsf.advanceLine();
	}
}

void GameSet::parseFile(const char * fn, int pass)
{
	static std::set<std::string, StriCompare> processedFiles;
	static int callcount = 0;
	callcount++;
	processedFiles.insert(fn);
	//printf("Processing %s\n", fn);

	std::string directory;
	if (const char *lastbs = strrchr(fn, '\\')) {
		directory = std::string(fn, lastbs - fn + 1);
	}

	char *text; int filesize;
	LoadFile(fn, &text, &filesize, 1);
	text[filesize] = 0;
	GSFileParser gsf(text);

	std::vector<std::string> linkedFiles;

	int linecnt = 1;
	while (!gsf.eof)
	{
		//printf("Line %i\n", linecnt++);
		std::string strtag = gsf.nextTag();
		int tag = Tags::GAMESET_tagDict.getTagID(strtag.c_str());

		if (tag == Tags::GAMESET_LINK_GAME_SET)
		{
			std::string lf = gsf.nextString(true);
			if(lf.find('*') == std::string::npos)
				linkedFiles.push_back(lf);
		}
		else if (pass == 0) {
			switch (tag) {
			case Tags::GAMESET_DECLARE_ITEM: {
				itemNames.insertString(gsf.nextString(true));
				break;
			}
			case Tags::GAMESET_DEFINE_VALUE: {
				std::string tag = gsf.nextString(true);
				definedValues[tag] = gsf.nextFloat();
				break;
			}
			case Tags::GAMESET_APPEARANCE_TAG: {
				appearanceNames.insertString(gsf.nextString(true));
				break;
			}
			case Tags::GAMESET_EQUATION: {
				equationNames.insertString(gsf.nextString(true));
				ignoreBlueprint(gsf, strtag);
				break;
			}
			case Tags::GAMESET_ACTION_SEQUENCE: {
				actionSequenceNames.insertString(gsf.nextString(true));
				ignoreBlueprint(gsf, strtag);
				break;
			}
			case Tags::GAMESET_COMMAND: {
				commandNames.insertString(gsf.nextString(true));
				ignoreBlueprint(gsf, strtag);
				break;
			}
			case Tags::GAMESET_ANIMATION_TAG: {
				animationNames.insertString(gsf.nextString(true));
				break;
			}
			case Tags::GAMESET_CHARACTER_LADDER: {
				ignoreBlueprint(gsf, strtag);
				break;
			}
			case Tags::GAMESET_LEVEL:
			case Tags::GAMESET_PLAYER:
			case Tags::GAMESET_CHARACTER:
			case Tags::GAMESET_BUILDING:
			case Tags::GAMESET_MARKER:
			case Tags::GAMESET_CONTAINER:
			case Tags::GAMESET_PROP:
			case Tags::GAMESET_CITY:
			case Tags::GAMESET_TOWN:
			{
				int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(strtag.c_str());
				objBlueprintNames[cls].insertString(gsf.nextString(true));
				ignoreBlueprint(gsf, strtag);
				break;
			}
			}
		}
		else if (pass == 1) {
			switch (tag) {
			case Tags::GAMESET_DECLARE_ITEM: {
				int x = itemNames.getIndex(gsf.nextString(true));
				int side = 0;
				itemSides[x] = side;
				break;
			}
			case Tags::GAMESET_LEVEL:
			case Tags::GAMESET_PLAYER:
			case Tags::GAMESET_CHARACTER:
			case Tags::GAMESET_BUILDING:
			case Tags::GAMESET_MARKER:
			case Tags::GAMESET_CONTAINER:
			case Tags::GAMESET_PROP:
			case Tags::GAMESET_CITY:
			case Tags::GAMESET_TOWN:
			{
				int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(strtag.c_str());
				std::string name = gsf.nextString(true);
				int bpx = objBlueprintNames[cls].getIndex(name);
				objBlueprints[cls][bpx].init(cls, bpx, name, this);
				objBlueprints[cls][bpx].parse(gsf, directory);
				break;
			}
			case Tags::GAMESET_EQUATION: {
				int x = equationNames.getIndex(gsf.nextString(true));
				equations[x] = ReadEquationNode(gsf, *this);
				break;
			}
			case Tags::GAMESET_ACTION_SEQUENCE: {
				int x = actionSequenceNames.getIndex(gsf.nextString(true));
				actionSequences[x].init(gsf, *this, "END_ACTION_SEQUENCE");
				break;
			}
			case Tags::GAMESET_COMMAND: {
				int x = commandNames.getIndex(gsf.nextString(true));
				commands[x].id = x;
				commands[x].parse(gsf, *this);
				break;
			}
			case Tags::GAMESET_CHARACTER_LADDER: {
				ignoreBlueprint(gsf, strtag);
				break;
			}
			}
		}

		gsf.advanceLine();
	}

	free(text);

	for (std::string &lf : linkedFiles) {
		std::string lfpath = "Warrior Kings Game Set\\" + lf;
		if (!processedFiles.count(lfpath))
			parseFile(lfpath.c_str(), pass);
	}

	if (--callcount == 0)
		processedFiles.clear();
}

void GameSet::load(const char * fn)
{
	appearanceNames.insertString("Default");
	animationNames.insertString("Default");

	printf("Gameset pass 1...\n");
	parseFile(fn, 0);

	for (int i = 0; i < Tags::GAMEOBJCLASS_COUNT; i++)
		objBlueprints[i] = std::unique_ptr<GameObjBlueprint[]>(new GameObjBlueprint[objBlueprintNames[i].size()]);
	itemSides = std::make_unique<int[]>(itemNames.size());
	equations = std::make_unique<ValueDeterminer*[]>(equationNames.size());
	actionSequences = std::make_unique<ActionSequence[]>(actionSequenceNames.size());
	commands = std::make_unique<Command[]>(commandNames.size());

	printf("Gameset pass 2...\n");
	parseFile(fn, 1);
	printf("Gameset loaded!\n");
}
