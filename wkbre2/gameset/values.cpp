
#include "values.h"
#include "gameset.h"
#include "../util/GSFileParser.h"
#include "../tags.h"
#include "../util/util.h"
#include <string>
#include "finder.h"
#include <algorithm>
#include "../server.h"
#include "../client.h"
#include "CommonEval.h"
#include "position.h"

namespace {
	float RandomFromZeroToOne() { return (float)(rand() & 0xFFFF) / 32768.0f; }
}

//namespace Script
//{

float ValueDeterminer::eval(ClientGameObject * self)
{
	ferr("eval(ClientGameObject*) unimplemented for this Value Determiner!");
	return 0.0f;
}

struct ValueUnknown : ValueDeterminer {
	virtual float eval(ServerGameObject *self) override { ferr("Unknown value determiner called from the Server!"); return 0.0f; }
	virtual float eval(ClientGameObject *self) override { ferr("Unknown value determiner called from the Client!"); return 0.0f; }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct ValueConstant : ValueDeterminer {
	float value;
	virtual float eval(ServerGameObject *self) override { return value; }
	virtual float eval(ClientGameObject *self) override { return value; }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override { value = gsf.nextFloat(); }
	ValueConstant() {}
	ValueConstant(float value) : value(value) {}
};

struct ValueItemValue : CommonEval<ValueItemValue, ValueDeterminer> {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		if (AnyGameObject* obj = finder->getFirst(self))
			return obj->getItem(item);
		return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueItemValue() {}
	ValueItemValue(int item, ObjectFinder *finder) : item(item), finder(finder) {}
};

struct ValueObjectId_Battles : CommonEval<ValueObjectId_Battles, ValueDeterminer> {
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		if (AnyGameObject *obj = finder->getFirst(self))
			return obj->id;
		return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueObjectId_Battles() {}
	ValueObjectId_Battles(ObjectFinder *finder) : finder(finder) {}
};

struct ValueObjectClass : CommonEval<ValueObjectClass, ValueDeterminer> {
	int objclass;
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		auto vec = finder->eval(self);
		return !vec.empty() && std::all_of(vec.begin(), vec.end(), [this](AnyGameObject *obj) {return obj->blueprint->bpClass == objclass; });
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		objclass = Tags::GAMEOBJCLASS_tagDict.getTagID(gsf.nextString().c_str());
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueObjectClass() {}
	ValueObjectClass(int objclass, ObjectFinder *finder) : objclass(objclass), finder(finder) {}
};

struct ValueEquationResult : CommonEval<ValueEquationResult, ValueDeterminer> {
	int equation;
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		AnyGameObject *obj = finder->getFirst(self);
		if (!obj) return 0.0f;
		return AnyGameObject::Program::instance->gameSet->equations[equation]->eval(obj);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	//ValueEquationResult() {}
	//ValueEquationResult(int equation, ObjectFinder *finder) : equation(equation), finder(finder) {}
};

struct ValueIsSubsetOf : CommonEval<ValueIsSubsetOf, ValueDeterminer> {
	std::unique_ptr<ObjectFinder> fnd_sub, fnd_super;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		auto sub = fnd_sub->eval(self);
		auto super = fnd_super->eval(self);
		if (sub.empty() || super.empty())
			return 0.0f;
		for (auto *obj : sub)
			if (std::find(super.begin(), super.end(), obj) == super.end())
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		fnd_sub.reset(ReadFinder(gsf, gs));
		fnd_super.reset(ReadFinder(gsf, gs));
	}
};

struct ValueNumObjects : CommonEval<ValueNumObjects, ValueDeterminer> {
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		return finder->eval(self).size();
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueCanReach : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<PositionDeterminer> source;
	virtual float eval(ServerGameObject *self) override {
		// everything is reachable from anywhere for now
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
		source.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ValueHasAppearance : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	int appear;
	virtual float eval(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			if (obj->appearance != appear)
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
		appear = gs.appearances.readIndex(gsf);
	}
};

struct ValueSamePlayer : CommonEval<ValueSamePlayer, ValueDeterminer> {
	std::unique_ptr<ObjectFinder> fnd1, fnd2;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		auto vec1 = fnd1->eval(self);
		auto vec2 = fnd2->eval(self);
		if (vec1.empty() || vec2.empty()) return 0.0f;
		for (AnyGameObject *b : vec2) {
			AnyGameObject *bp = b->getPlayer();
			if (std::find_if(vec1.begin(), vec1.end(), [&bp](AnyGameObject * a) {return a->getPlayer() == bp;}) == vec1.end())
				return 0.0f;
		}
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		fnd1.reset(ReadFinder(gsf, gs));
		fnd2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueObjectType : CommonEval<ValueObjectType, ValueDeterminer> {
	GameObjBlueprint *type;
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		auto vec = finder->eval(self);
		if (vec.empty()) return 0.0f;
		for (auto *obj : vec)
			if (obj->blueprint != type)
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		type = gs.readObjBlueprintPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueDistanceBetween : ValueDeterminer {
	bool takeHorizontal, takeVertical;
	std::unique_ptr<ObjectFinder> fnd1, fnd2;
	Vector3 centre(const std::vector<ServerGameObject*> &vec) {
		if (vec.empty()) return {};
		Vector3 sum;
		for (ServerGameObject *obj : vec)
			sum += obj->position;
		return sum / vec.size();
	}
	virtual float eval(ServerGameObject *self) override {
		Vector3 v = centre(fnd1->eval(self)) - centre(fnd2->eval(self));
		float dist = 0.0f;
		if (takeHorizontal) dist += v.x * v.x + v.z * v.z;
		if (takeVertical) dist += v.y * v.y;
		return std::sqrt(dist);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		auto tag = gsf.nextString();
		if (tag == "HORIZONTAL")	{ takeHorizontal = true;  takeVertical = false; }
		else if (tag == "3D")		{ takeHorizontal = true;  takeVertical = true; }
		else if (tag == "VERTICAL")	{ takeHorizontal = false; takeVertical = true; }
		fnd1.reset(ReadFinder(gsf, gs));
		fnd2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueValueTagInterpretation : ValueDeterminer {
	int valueTag;
	std::unique_ptr<ObjectFinder> fnd;
	virtual float eval(ServerGameObject *self) override {
		ServerGameObject *obj = fnd->getFirst(self);
		if (!obj) return 0.0f;
		// TODO: Type specific interpretations
		ValueDeterminer *vd = Server::instance->gameSet->valueTags[valueTag].get();
		if (!vd) return 0.0f;
		return vd->eval(obj);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		valueTag = gs.valueTags.readIndex(gsf);
		fnd.reset(ReadFinder(gsf, gs));
	}
};

struct ValueDiplomaticStatusAtLeast : CommonEval<ValueDiplomaticStatusAtLeast, ValueDeterminer> {
	int status;
	std::unique_ptr<ObjectFinder> a, b;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		AnyGameObject *x = a->getFirst(self), *y = b->getFirst(self);
		if (!(x && y)) return 0.0f;
		return AnyGameObject::Program::instance->getDiplomaticStatus(x->getPlayer(), y->getPlayer()) <= status;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		status = gs.diplomaticStatuses.readIndex(gsf);
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueAreAssociated : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> a, b;
	virtual float eval(ServerGameObject *self) override {
		ServerGameObject *x = a->getFirst(self), *y = b->getFirst(self);
		if (!(x && y)) return 0.0f;
		return x->associates[category].count(y) ? 1.0f : 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadFinder(gsf, gs));
		category = gs.associations.readIndex(gsf);
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueBlueprintItemValue : CommonEval<ValueBlueprintItemValue, ValueDeterminer> {
	int item;
	GameObjBlueprint *type;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		auto it = type->startItemValues.find(item);
		if (it != type->startItemValues.end())
			return it->second;
		else return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		type = gs.readObjBlueprintPtr(gsf);
	}
};

struct ValueTotalItemValue : CommonEval<ValueTotalItemValue, ValueDeterminer> {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		float sum = 0.0f;
		for (AnyGameObject* obj : finder->eval(self))
			sum += obj->getItem(item);
		return sum;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIsIdle : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ServerGameObject* self) override {
		auto vec = finder->eval(self);
		return std::all_of(vec.begin(), vec.end(), [](ServerGameObject* obj) {return obj->orderConfig.orders.empty(); });
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueFinderResultsCount : ValueDeterminer {
	int ofd;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ServerGameObject* self) override {
		ServerGameObject* obj = finder->getFirst(self);
		if (!obj) return 0.0f;
		return Server::instance->gameSet->objectFinderDefinitions[ofd]->eval(obj).size();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		ofd = gs.objectFinderDefinitions.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueHasDirectLineOfSightTo : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder1;
	std::unique_ptr<PositionDeterminer> pos;
	std::unique_ptr<ObjectFinder> finder2;
	virtual float eval(ServerGameObject* self) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder1.reset(ReadFinder(gsf, gs));
		pos.reset(PositionDeterminer::createFrom(gsf, gs));
		finder2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueWaterBeneath : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ServerGameObject* self) override {
		// TODO
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueAngleBetween : ValueDeterminer {
	int mode;
	std::unique_ptr<ObjectFinder> a, b;
	virtual float eval(ServerGameObject* self) override {
		// TODO (apparently it always returns 0?)
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gsf.nextString();
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIsMusicPlaying : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ServerGameObject* self) override {
		// TODO
		// For now, let's make it return 1 so that the Maestro army won't try to constantly change the music
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs)
{
	ValueDeterminer *vd;
	switch (Tags::VALUE_tagDict.getTagID(gsf.nextTag().c_str())) {
	case Tags::VALUE_CONSTANT: vd = new ValueConstant(gsf.nextFloat()); return vd;
	case Tags::VALUE_DEFINED_VALUE: vd = new ValueConstant(gs.definedValues.at(gsf.nextString(true))); return vd;
	case Tags::VALUE_ITEM_VALUE: vd = new ValueItemValue; break;
	case Tags::VALUE_OBJECT_ID: vd = new ValueObjectId_Battles; break;
	case Tags::VALUE_OBJECT_CLASS: vd = new ValueObjectClass; break;
	case Tags::VALUE_EQUATION_RESULT: vd = new ValueEquationResult; break;
	case Tags::VALUE_IS_SUBSET_OF: vd = new ValueIsSubsetOf; break;
	case Tags::VALUE_NUM_OBJECTS: vd = new ValueNumObjects; break;
	case Tags::VALUE_CAN_REACH: vd = new ValueCanReach; break;
	case Tags::VALUE_HAS_APPEARANCE: vd = new ValueHasAppearance; break;
	case Tags::VALUE_SAME_PLAYER: vd = new ValueSamePlayer; break;
	case Tags::VALUE_OBJECT_TYPE: vd = new ValueObjectType; break;
	case Tags::VALUE_DISTANCE_BETWEEN: vd = new ValueDistanceBetween; break;
	case Tags::VALUE_VALUE_TAG_INTERPRETATION: vd = new ValueValueTagInterpretation; break;
	case Tags::VALUE_DIPLOMATIC_STATUS_AT_LEAST: vd = new ValueDiplomaticStatusAtLeast; break;
	case Tags::VALUE_ARE_ASSOCIATED: vd = new ValueAreAssociated; break;
	case Tags::VALUE_BLUEPRINT_ITEM_VALUE: vd = new ValueBlueprintItemValue; break;
	case Tags::VALUE_TOTAL_ITEM_VALUE: vd = new ValueTotalItemValue; break;
	case Tags::VALUE_IS_IDLE: vd = new ValueIsIdle; break;
	case Tags::VALUE_FINDER_RESULTS_COUNT: vd = new ValueFinderResultsCount; break;
	case Tags::VALUE_HAS_DIRECT_LINE_OF_SIGHT_TO: vd = new ValueHasDirectLineOfSightTo; break;
	case Tags::VALUE_WATER_BENEATH: vd = new ValueWaterBeneath; break;
	case Tags::VALUE_ANGLE_BETWEEN: vd = new ValueAngleBetween; break;
	case Tags::VALUE_IS_MUSIC_PLAYING: vd = new ValueIsMusicPlaying; break;
	default: vd = new ValueUnknown; break;
	}
	vd->parse(gsf, const_cast<GameSet&>(gs));
	return vd;
}

// Unary equation nodes

struct UnaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeNot : CommonEval<EnodeNot, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) <= 0.0f; }
};

struct EnodeIsZero : CommonEval<EnodeIsZero, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) == 0.0f; }
};

struct EnodeIsPositive : CommonEval<EnodeIsPositive, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) > 0.0f; }
};

struct EnodeIsNegative : CommonEval<EnodeIsNegative, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) < 0.0f; }
};

struct EnodeAbsoluteValue : CommonEval<EnodeAbsoluteValue, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return std::abs(a->eval(self)); }
};

struct EnodeNegate : CommonEval<EnodeNegate, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return -a->eval(self); }
};

struct EnodeRandomUpTo : CommonEval<EnodeRandomUpTo, UnaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject* self) { return RandomFromZeroToOne() * a->eval(self); }
};

// Binary equation nodes

struct BinaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeAddition : CommonEval<EnodeAddition, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) + b->eval(self); }
};

struct EnodeSubtraction : CommonEval<EnodeSubtraction, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) - b->eval(self); }
};

struct EnodeMultiplication : CommonEval<EnodeMultiplication, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) * b->eval(self); }
};

struct EnodeDivision : CommonEval<EnodeDivision, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) / b->eval(self); }
};

struct EnodeAnd : CommonEval<EnodeAnd, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return (a->eval(self) > 0.0f) && (b->eval(self) > 0.0f); }
};

struct EnodeOr : CommonEval<EnodeOr, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return (a->eval(self) > 0.0f) || (b->eval(self) > 0.0f); }
};

struct EnodeLessThan : CommonEval<EnodeLessThan, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) < b->eval(self); }
};

struct EnodeLessThanOrEqualTo : CommonEval<EnodeLessThanOrEqualTo, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) <= b->eval(self); }
};

struct EnodeGreaterThan : CommonEval<EnodeGreaterThan, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) > b->eval(self); }
};

struct EnodeGreaterThanOrEqualTo : CommonEval<EnodeGreaterThanOrEqualTo, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) >= b->eval(self); }
};

struct EnodeEquals : CommonEval<EnodeEquals, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return a->eval(self) == b->eval(self); }
};

struct EnodeMax : CommonEval<EnodeMax, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return std::max(a->eval(self), b->eval(self)); }
};

struct EnodeMin : CommonEval<EnodeMin, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) { return std::min(a->eval(self), b->eval(self)); }
};

struct EnodeRandomInteger : CommonEval<EnodeRandomInteger, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		int x = (int)a->eval(self), y = (int)b->eval(self);
		return (float)(rand() % (y - x + 1) + x);
	}
};

struct EnodeRandomRange : CommonEval<EnodeRandomRange, BinaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject* self) {
		float x = a->eval(self), y = b->eval(self);
		return RandomFromZeroToOne() * (y - x) + x;
	}
};

// Ternary equation nodes

struct TernaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b, c;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
		c.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeIfThenElse : CommonEval<EnodeIfThenElse, TernaryEnode> {
	template<typename AnyGameObject> float common_eval(AnyGameObject *self) {
		if (a->eval(self) > 0.0f)
			return b->eval(self);
		else
			return c->eval(self);
	}
};

ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		if (strtag == "END_EQUATION")
			return new ValueUnknown();
		else if (strtag == "ENODE") {
			const char *oldcur = gsf.cursor;
			ValueDeterminer *vd;
			switch (Tags::ENODE_tagDict.getTagID(gsf.nextString().c_str())) {
			case Tags::ENODE_ADDITION: vd = new EnodeAddition; break;
			case Tags::ENODE_NEGATE: vd = new EnodeNegate; break;
			case Tags::ENODE_SUBTRACTION: vd = new EnodeSubtraction; break;
			case Tags::ENODE_MULTIPLICATION: vd = new EnodeMultiplication; break;
			case Tags::ENODE_DIVISION: vd = new EnodeDivision; break;
			case Tags::ENODE_NOT: vd = new EnodeNot; break;
			case Tags::ENODE_IS_ZERO: vd = new EnodeIsZero; break;
			case Tags::ENODE_IS_POSITIVE: vd = new EnodeIsPositive; break;
			case Tags::ENODE_IS_NEGATIVE: vd = new EnodeIsNegative; break;
			case Tags::ENODE_ABSOLUTE_VALUE: vd = new EnodeAbsoluteValue; break;
			case Tags::ENODE_RANDOM_UP_TO: vd = new EnodeRandomUpTo; break;
			case Tags::ENODE_AND: vd = new EnodeAnd; break;
			case Tags::ENODE_OR: vd = new EnodeOr; break;
			case Tags::ENODE_LESS_THAN: vd = new EnodeLessThan; break;
			case Tags::ENODE_LESS_THAN_OR_EQUAL_TO: vd = new EnodeLessThanOrEqualTo; break;
			case Tags::ENODE_GREATER_THAN: vd = new EnodeGreaterThan; break;
			case Tags::ENODE_GREATER_THAN_OR_EQUAL_TO: vd = new EnodeGreaterThanOrEqualTo; break;
			case Tags::ENODE_EQUALS: vd = new EnodeEquals; break;
			case Tags::ENODE_MAX: vd = new EnodeMax; break;
			case Tags::ENODE_MIN: vd = new EnodeMin; break;
			case Tags::ENODE_RANDOM_INTEGER: vd = new EnodeRandomInteger; break;
			case Tags::ENODE_RANDOM_RANGE: vd = new EnodeRandomRange; break;
			case Tags::ENODE_IF_THEN_ELSE: vd = new EnodeIfThenElse; break;
			default: 
				gsf.cursor = oldcur;
				return ReadValueDeterminer(gsf, gs);
			}
			vd->parse(gsf, const_cast<GameSet&>(gs));
			return vd;
		}
		gsf.advanceLine();
	}
	ferr("Equation reached end of file without END_EQUATION!");
	return nullptr;
}

//}