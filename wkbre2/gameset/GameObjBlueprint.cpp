#include "GameObjBlueprint.h"
#include "gameset.h"
#include "../tags.h"
#include <string>
#include "../file.h"

void GameObjBlueprint::loadAnimations(GameObjBlueprint::BPAppearance &ap, const std::string &dir, bool overrideAnims) {
	//printf("Loading anims from %s\n", dir.c_str());
	std::vector<std::string> gsl;
	ListFiles(dir.c_str(), &gsl);
	std::set<int> alreadyOverridenAnims;
	for (const std::string &fn : gsl) {
		size_t pp = fn.find('.');
		std::string name = fn.substr(0, pp);
		std::string ext = fn.substr(pp + 1);
		if (!_stricmp(ext.c_str(), "MESH3") || !_stricmp(ext.c_str(), "ANIM3")) {
			int pn = name.find_first_of("0123456789");
			std::string ats = name.substr(0, pn);
			//std::string num = name.substr(pn);
			//printf("? %s\n", ats.c_str());
			int animTag = this->gameSet->animations.names.getIndex(ats);
			if (animTag != -1) {
				if(overrideAnims)
					if (!alreadyOverridenAnims.count(animTag)) {
						alreadyOverridenAnims.insert(animTag);
						ap.animations[animTag].clear();
					}
				//printf("Found %s\n", fn.c_str());
				ap.animations[animTag].push_back(this->gameSet->modelCache.getModel(dir + fn));
			}
		}
		//printf("%s %s\n", ext.c_str(), name.c_str());
	}
	// Ensure first variant of animation is an anim3 (prob bad trick, fixes birds having static mesh as first variant)
	for (auto &anim : ap.animations) {
		for (auto &mod : anim.second) {
			if (dynamic_cast<AnimatedModel*>(mod)) {
				std::swap(anim.second.front(), mod);
				break;
			}
		}
	}
}

void GameObjBlueprint::parse(GSFileParser & gsf, const std::string &directory, bool isExtension) {
	//printf("Parsing blueprint \"%s\"\n", this->name.c_str());
	// TODO: read model path
	modelPath = gsf.nextString(true);
	gsf.advanceLine();
	std::string endtag("END_");
	endtag += Tags::GAMEOBJCLASS_tagDict.getStringFromID(bpClass);
	if (isExtension)
		endtag += "_EXTENSION";
	while (!gsf.eof)
	{
		std::string strtag = gsf.nextString();
		int tag = Tags::CBLUEPRINT_tagDict.getTagID(strtag.c_str());
		if (strtag == endtag)
			break;
		switch (tag) {
		case Tags::CBLUEPRINT_STARTS_WITH_ITEM: {
			int x = gameSet->items.readIndex(gsf);
			this->startItemValues[x] = gsf.nextFloat();
			break;
		}
		case Tags::CBLUEPRINT_PHYSICAL_SUBTYPE: {
			std::string sts = gsf.nextString(true);
			subtypeNames.insertString(sts);
			PhysicalSubtype &ps = this->subtypes[subtypeNames.getIndex(sts)];
			ps.dir = gsf.nextString(true);
			if(!ps.dir.empty())
				if (ps.dir.back() != '\\')
					ps.dir += '\\';
			gsf.advanceLine();
			while (!gsf.eof) {
				std::string apphead = gsf.nextTag();
				if (apphead == "END_PHYSICAL_SUBTYPE")
					break;
				else if (apphead == "APPEARANCE") {
					std::string appstr = gsf.nextString(true);
					int appid = gameSet->appearances.names.getIndex(appstr);
					gsf.advanceLine();
					BPAppearance &ap = ps.appearances[appid];
					while (!gsf.eof) {
						std::string ldfstr = gsf.nextTag();
						if (ldfstr == "END_APPEARANCE")
							break;
						else if (ldfstr == "LOAD_ANIMATIONS_FROM") {
							ap.dir = gsf.nextString(true);
							bool over = gsf.nextString() == "OVERRIDE";
							loadAnimations(ap, directory + modelPath + ps.dir + ap.dir, over);
						}
						gsf.advanceLine();
					}
				}
				else if (apphead == "MAP_SOUND_TAG") {
					int st = gameSet->soundTags.readIndex(gsf);
					int unk1 = gsf.nextInt();
					auto unk2 = gsf.nextString();
					ps.soundMap[st].push_back(gsf.nextString(true));
				}
				else if (apphead == "MAP_SOUND_TAG_TO") {
					int st = gameSet->soundTags.readIndex(gsf);
					int unk1 = gsf.nextInt();
					auto unk2 = gsf.nextString();
					ps.soundMap[st].push_back(gameSet->sounds.readIndex(gsf));
				}
				gsf.advanceLine();
			}
			// If there is no Default appearance, make one
			if (ps.appearances.count(0) == 0) {
				loadAnimations(ps.appearances[0], directory + modelPath + ps.dir);
			}
			break;
		}
		case Tags::CBLUEPRINT_OFFERS_COMMAND: {
			int x = gameSet->commands.readIndex(gsf);
			if(x != -1) // TODO: WARNING
				offeredCommands.push_back(&gameSet->commands[x]);
			break;
		}
		case Tags::CBLUEPRINT_INTRINSIC_REACTION: {
			if (Reaction* react = gameSet->reactions.readPtr(gsf)) // TODO: WARNING
				intrinsicReactions.push_back(react);
			break;
		}
		case Tags::CBLUEPRINT_MAP_TYPE_TAG: {
			int tag = gameSet->typeTags.readIndex(gsf);
			mappedTypeTags[tag] = gameSet->readObjBlueprintPtr(gsf);
			break;
		}
		case Tags::CBLUEPRINT_REPRESENT_AS: {
			representAs = gameSet->modelCache.getModel("Warrior Kings Game Set\\" + gsf.nextString(true));
			break;
		}
		case Tags::CBLUEPRINT_INTERPRET_VALUE_TAG_AS: {
			int vt = gameSet->valueTags.readIndex(gsf);
			mappedValueTags[vt] = ReadValueDeterminer(gsf, *gameSet);
			break;
		}
		case Tags::CBLUEPRINT_MISSILE_SPEED: {
			missileSpeed = ReadValueDeterminer(gsf, *gameSet);
			break;
		}
		case Tags::CBLUEPRINT_MAP_SOUND_TAG: {
			int st = gameSet->soundTags.readIndex(gsf);
			int unk1 = gsf.nextInt();
			//assert(unk1 == 1);
			auto unk2 = gsf.nextString();
			//assert(unk2 == "NEAR");
			soundMap[st].push_back(gsf.nextString(true));
			break;
		}
		case Tags::CBLUEPRINT_MAP_SOUND_TAG_TO: {
			int st = gameSet->soundTags.readIndex(gsf);
			int unk1 = gsf.nextInt();
			auto unk2 = gsf.nextString();
			soundMap[st].push_back(gameSet->sounds.readIndex(gsf));
			break;
		}
		case Tags::CBLUEPRINT_MAP_MUSIC_TAG: {
			int mt = gameSet->musicTags.readIndex(gsf);
			musicMap[mt].push_back(gsf.nextString(true));
			break;
		}
		}
		gsf.advanceLine();
	}
	// If there are no subtypes, make a Default subtype with one Default appearance
	if(!isExtension && this->subtypes.empty()) {
		this->subtypeNames.insertString("Default");
		loadAnimations(this->subtypes[0].appearances[0], directory + modelPath);
	}
}

std::string GameObjBlueprint::getFullName() {
	return std::string(Tags::GAMEOBJCLASS_tagDict.getStringFromID(bpClass)) + " \"" + name + "\"";
}
