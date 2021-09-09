#pragma once

#include "../util/GSFileParser.h"
//#include "gameset.h"
#include <map>
#include <string>
#include <vector>
#include "../util/IndexedStringList.h"
#include "../util/vecmat.h"

struct GameSet;
struct Command;
struct Model;
struct Reaction;
struct ValueDeterminer;
struct Footprint;

struct GameObjBlueprint {
	struct BPAppearance {
		std::string dir;
		std::map<int, std::vector<Model*>> animations;
	};

	struct SoundRef {
		std::string filePath;
		int soundBlueprint = -1;
		SoundRef(std::string&& name) : filePath(name) {}
		SoundRef(int sndBp) : soundBlueprint(sndBp) {}
	};

	struct PhysicalSubtype {
		std::string dir;
		std::map<int, BPAppearance> appearances;
		std::map<int, std::vector<SoundRef>> soundMap;
	};

	struct MovementBand {
		float naturalMovementSpeed = 1.0f;
		int defaultAnim = -1;
		std::vector<std::pair<int, int>> onEquAnims;
	};

	int bpClass, bpId;
	std::string name;
	GameSet *gameSet;

	std::map<int, float> startItemValues;

	IndexedStringList subtypeNames;
	std::map<int, PhysicalSubtype> subtypes;
	std::string modelPath;

	std::vector<Command*> offeredCommands;

	std::vector<Reaction*> intrinsicReactions;

	std::map<int, GameObjBlueprint*> mappedTypeTags;
	std::map<int, ValueDeterminer*> mappedValueTags;

	Model* representAs = nullptr;

	ValueDeterminer* missileSpeed = nullptr;
	ValueDeterminer* doNotCollideWith = nullptr;
	int strikeFloorSound = -1;
	int strikeWaterSound = -1;

	std::map<int, std::vector<SoundRef>> soundMap;
	std::map<int, std::vector<std::string>> musicMap;
	std::map<int, std::vector<Model*>> specialEffectMap;

	bool generateSightRangeEvents = false;
	bool receiveSightRangeEvents = false;
	ValueDeterminer* shouldProcessSightRange = nullptr;
	int sightRangeEquation = -1;

	Footprint* footprint = nullptr;

	Vector3 scaleAppearance = Vector3(1, 1, 1);
	Vector3 minScaleVary = Vector3(1, 1, 1);
	Vector3 maxScaleVary = Vector3(1, 1, 1);

	bool floatsOnWater = false;

	bool removeWhenNotReferenced = false;

	int movementSpeedEquation = -1;
	std::vector<MovementBand> movementBands;

	void parse(GSFileParser &gsf, const std::string &directory, bool isExtension = false);
	void init(int i_bpClass, int i_bpId, const std::string &i_name, GameSet *i_gameSet) {
		bpClass = i_bpClass; bpId = i_bpId; name = i_name; gameSet = i_gameSet;
	}

	uint32_t getFullId() { return bpClass | (bpId << 6); }
	std::string getFullName();

	std::tuple<std::string, float, float> getSound(int sndTag, int subtype);
	Model* getModel(int subtype, int appear, int anim, int variant);
	Model* getSpecialEffect(int sfxTag);

	bool canWalkOnWater() const;

	//GameObjBlueprint() {}
	//GameObjBlueprint(GameSet *gameSet) : gameSet(gameSet) {}
	//GameObjBlueprint(int cls, const std::string &name, GameSet &gameSet) : bpClass(cls), name(name), gameSet(gameSet) {}

private:
	void loadAnimations(GameObjBlueprint::BPAppearance &ap, const std::string &dir, bool overrideAnims = false);
};
