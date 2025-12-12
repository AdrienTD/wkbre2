// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "../util/GSFileParser.h"
//#include "gameset.h"
#include <map>
#include <cstdint>
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
struct ObjectFinder;

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

	std::map<int, const GameObjBlueprint*> mappedTypeTags;
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

	ObjectFinder* aiSpawnLocationSelector = nullptr;

	int buildingType = -1;

	bool objectIsRenderable = true;
	bool objectIsSelectable = true;
	bool objectIsTargetable = true;

	void parse(GSFileParser &gsf, const std::string &directory, bool isExtension = false);
	void init(int i_bpClass, int i_bpId, const std::string &i_name, GameSet *i_gameSet) {
		bpClass = i_bpClass; bpId = i_bpId; name = i_name; gameSet = i_gameSet;
	}

	uint32_t getFullId() const;
	std::string getFullName() const;

	std::tuple<std::string, float, float> getSound(int sndTag, int subtype) const;
	const BPAppearance* getAppearance(int subtype, int appear) const;
	Model* getModel(int subtype, int appear, int anim, int variant) const;
	Model* getSpecialEffect(int sfxTag) const;

	bool canWalkOnWater() const;

	float getStartingItemValue(int itemIndex) const;
	int getStartingFlags() const;

	//GameObjBlueprint() {}
	//GameObjBlueprint(GameSet *gameSet) : gameSet(gameSet) {}
	//GameObjBlueprint(int cls, const std::string &name, GameSet &gameSet) : bpClass(cls), name(name), gameSet(gameSet) {}

private:
	void loadAnimations(GameObjBlueprint::BPAppearance &ap, const std::string &dir, bool overrideAnims = false);
};

class GameObjBlueprintIndex
{
public:
	GameObjBlueprintIndex(int classTag, int id) : m_data(id | (classTag << 24)) {}
	GameObjBlueprintIndex(const GameObjBlueprint* blueprint) : GameObjBlueprintIndex(blueprint->bpClass, blueprint->bpId) {}
	int bpId() const { return m_data & 0xFFFFFF; }
	int bpClass() const { return (m_data >> 24) & 0xFF; }

	uint32_t fullId() const { return m_data; }
	static GameObjBlueprintIndex fromFullId(uint32_t data) { return GameObjBlueprintIndex(data); }

	bool operator<(const GameObjBlueprintIndex& other) const { return m_data < other.m_data; }
	bool operator==(const GameObjBlueprintIndex& other) const { return m_data == other.m_data; }
private:
	GameObjBlueprintIndex(uint32_t data) : m_data(data) {}
	uint32_t m_data;
};

inline uint32_t GameObjBlueprint::getFullId() const { return GameObjBlueprintIndex(this).fullId(); }
