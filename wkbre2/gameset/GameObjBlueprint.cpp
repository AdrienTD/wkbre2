// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "GameObjBlueprint.h"
#include "gameset.h"
#include "../tags.h"
#include <string>
#include "../file.h"
#include "finder.h"

void GameObjBlueprint::loadAnimations(GameObjBlueprint::BPAppearance &ap, const std::string &dir, bool overrideAnims) {
	//printf("Loading anims from %s\n", dir.c_str());
	std::vector<std::string> gsl;
	ListFiles(dir.c_str(), &gsl);
	std::set<int> alreadyOverridenAnims;
	for (const std::string &fn : gsl) {
		size_t pp = fn.find('.');
		std::string name = fn.substr(0, pp);
		std::string ext = fn.substr(pp + 1);
		if (!StrCICompare(ext, "mesh3") || !StrCICompare(ext, "anim3")) {
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
			// If there is no appearance, make a default one
			if (ps.appearances.empty()) {
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
		case Tags::CBLUEPRINT_GENERATE_SIGHT_RANGE_EVENTS: {
			generateSightRangeEvents = true; break;
		}
		case Tags::CBLUEPRINT_RECEIVE_SIGHT_RANGE_EVENTS: {
			receiveSightRangeEvents = true; break;
		}
		case Tags::CBLUEPRINT_SHOULD_PROCESS_SIGHT_RANGE: {
			shouldProcessSightRange = ReadValueDeterminer(gsf, *gameSet); break;
		}
		case Tags::CBLUEPRINT_SIGHT_RANGE_EQUATION: {
			sightRangeEquation = gameSet->equations.readIndex(gsf); break;
		}
		case Tags::CBLUEPRINT_STRIKE_FLOOR_SOUND: {
			strikeFloorSound = gameSet->soundTags.readIndex(gsf); break;
		}
		case Tags::CBLUEPRINT_STRIKE_WATER_SOUND: {
			strikeWaterSound = gameSet->soundTags.readIndex(gsf); break;
		}
		case Tags::CBLUEPRINT_DO_NOT_COLLIDE_WITH: {
			doNotCollideWith = ReadValueDeterminer(gsf, *gameSet); break;
		}
		case Tags::CBLUEPRINT_MAP_SPECIAL_EFFECT_TAG: {
			int tag = gameSet->specialEffectTags.readIndex(gsf);
			specialEffectMap[tag].push_back(gameSet->modelCache.getModel("Warrior Kings Game Set\\" + gsf.nextString(true)));
			break;
		}
		case Tags::CBLUEPRINT_CAN_SPAWN: {
			auto name = gsf.nextString(true);
			int x = gameSet->commands.names.getIndex("Spawn " + name);
			if (x != -1)
				offeredCommands.push_back(&gameSet->commands[x]);
			break;
		}
		case Tags::CBLUEPRINT_INHERITS_FROM: {
			GameObjBlueprint* base = gameSet->objBlueprints[bpClass].readPtr(gsf);
			auto backup_bpId = bpId;
			auto backup_name = name;
			*this = *base;
			bpId = backup_bpId;
			name = backup_name;
			break;
		}
		case Tags::CBLUEPRINT_USE_FOOTPRINT: {
			footprint = gameSet->footprints.readPtr(gsf);
			break;
		}
		case Tags::CBLUEPRINT_SCALE_APPEARANCE: {
			for (auto& f : scaleAppearance)
				f = gsf.nextFloat();
			break;
		}
		case Tags::CBLUEPRINT_VARY_X_SCALE: {
			minScaleVary.x = gsf.nextFloat();
			maxScaleVary.x = gsf.nextFloat();
			break;
		}
		case Tags::CBLUEPRINT_VARY_Y_SCALE: {
			minScaleVary.y = gsf.nextFloat();
			maxScaleVary.y = gsf.nextFloat();
			break;
		}
		case Tags::CBLUEPRINT_VARY_Z_SCALE: {
			minScaleVary.z = gsf.nextFloat();
			maxScaleVary.z = gsf.nextFloat();
			break;
		}
		case Tags::CBLUEPRINT_FLOATS_ON_WATER: {
			floatsOnWater = true;
			break;
		}
		case Tags::CBLUEPRINT_REMOVE_WHEN_NOT_REFERENCED: {
			removeWhenNotReferenced = true;
			break;
		}
		case Tags::CBLUEPRINT_MOVEMENT_SPEED_EQUATION: {
			movementSpeedEquation = gameSet->equations.readIndex(gsf);
			break;
		}
		case Tags::CBLUEPRINT_MOVEMENT_BAND: {
			movementBands.emplace_back();
			MovementBand& mb = movementBands.back();
			gsf.advanceLine();
			while (!gsf.eof) {
				auto btag = gsf.nextTag();
				if (btag == "NATURAL_MOVEMENT_SPEED")
					mb.naturalMovementSpeed = gsf.nextFloat();
				else if (btag == "PLAY_ANIMATION") {
					auto btype = gsf.nextString();
					if (btype == "TAG") {
						mb.defaultAnim = gameSet->animations.readIndex(gsf);
					}
					else if (btype == "FROM_SELECTION") {
						gsf.advanceLine();
						while (!gsf.eof) {
							auto stag = gsf.nextTag();
							if (stag == "ON_EQUATION_SUCCESS") {
								int eq = gameSet->equations.readIndex(gsf);
								int anim = gameSet->animations.readIndex(gsf);
								mb.onEquAnims.emplace_back(eq, anim);
							}
							else if (stag == "DEFAULT_ANIMATION") {
								mb.defaultAnim = gameSet->animations.readIndex(gsf);
							}
							else if (stag == "END_PLAY_ANIMATION")
								break;
							gsf.advanceLine();
						}
					}
				}
				else if (btag == "END_MOVEMENT_BAND")
					break;
				gsf.advanceLine();
			}
			break;
		}
		case Tags::CBLUEPRINT_CAN_BUILD: {
			auto name = gsf.nextString(true);
			int x = gameSet->commands.names.getIndex("Build " + name);
			if (x != -1)
				offeredCommands.push_back(&gameSet->commands[x]);
			break;
		}
		case Tags::CBLUEPRINT_BUILDING_TYPE: {
			buildingType = Tags::BUILDINGTYPE_tagDict.getTagID(gsf.nextString(false).c_str());
			break;
		}
		}
		if (strtag == "AI__SPAWN_LOCATION_SELECTOR") {
			aiSpawnLocationSelector = ReadFinder(gsf, *gameSet);
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

std::tuple<std::string, float, float> GameObjBlueprint::getSound(int sndTag, int subtype)
{
	const std::vector<GameObjBlueprint::SoundRef>* sndVars = nullptr;
	auto it = soundMap.find(sndTag);
	if (it != soundMap.end()) {
		sndVars = &it->second;
	}
	else {
		printf("from->subtype: %i\n", subtype);
		auto& pssm = subtypes.at(subtype).soundMap;
		it = pssm.find(sndTag);
		if (it != pssm.end())
			sndVars = &it->second;
	}
	if (!sndVars) {
		it = gameSet->globalSoundMap.find(sndTag);
		if (it != gameSet->globalSoundMap.end())
			sndVars = &it->second;
	}
	if (sndVars) {
		const std::string* path = nullptr;
		float refDist = 30.0f; float maxDist = 300.0f;
		const GameObjBlueprint::SoundRef& sndref = sndVars->at(rand() % sndVars->size());
		if (sndref.soundBlueprint != -1) {
			GSSound& snd = gameSet->sounds[sndref.soundBlueprint];
			refDist = snd.gradientStartDist;
			//maxDist = snd.muteDist;
			if (snd.files.size() > 0)
				path = &snd.files[rand() % snd.files.size()];
		}
		else if (!sndref.filePath.empty()) {
			path = &sndref.filePath;
		}
		if (path)
			return { *path, refDist, maxDist };
	}
	return {};
}

Model* GameObjBlueprint::getModel(int subtype, int appearance, int animationIndex, int animationVariant)
{
	const auto& ap = subtypes[subtype].appearances[appearance];
	auto it = ap.animations.find(animationIndex);
	if (it == ap.animations.end())
		it = ap.animations.find(0);
	if (it != ap.animations.end()) {
		const auto& anim = it->second;
		if (!anim.empty())
			return anim[animationVariant];
	}
	return nullptr;
}

Model* GameObjBlueprint::getSpecialEffect(int sfxTag)
{
	auto it = specialEffectMap.find(sfxTag);
	std::vector<Model*>* vec;
	if (it == specialEffectMap.end())
		vec = &gameSet->specialEffectTags[sfxTag];
	else
		vec = &it->second;
	if (!vec->empty()) {
		return (*vec)[rand() % vec->size()];
	}
	return nullptr;
}

bool GameObjBlueprint::canWalkOnWater() const { return floatsOnWater || bpClass != Tags::GAMEOBJCLASS_CHARACTER; }
