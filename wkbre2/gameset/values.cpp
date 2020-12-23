
#include "values.h"
#include "gameset.h"
#include "../util/GSFileParser.h"
#include "../tags.h"
#include "../util/util.h"
#include <string>
#include "finder.h"
#include <algorithm>

//namespace Script
//{
struct ValueUnknown : ValueDeterminer {
	virtual float eval(ServerGameObject *self) { ferr("Unknown value determiner called!"); return 0.0f; }
};

struct ValueConstant : ValueDeterminer {
	float value;
	virtual float eval(ServerGameObject *self) { return value; }
	ValueConstant(float value) : value(value) {}
};

struct ValueItemValue : ValueDeterminer {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ServerGameObject *self)
	{
		if (ServerGameObject* obj = finder->getFirst(self))
			return obj->getItem(item);
		return 0.0f;
	}
	ValueItemValue(int item, ObjectFinder *finder) : item(item), finder(finder) {}
};

struct ValueObjectId_Battles : ValueDeterminer {
	ObjectFinder *finder;
	float eval(ServerGameObject *self) {
		if (ServerGameObject *obj = finder->getFirst(self))
			return obj->id;
		return 0.0f;
	}
	ValueObjectId_Battles(ObjectFinder *finder) : finder(finder) {}
};

struct ValueObjectClass : ValueDeterminer {
	int objclass;
	ObjectFinder *finder;
	float eval(ServerGameObject *self) {
		auto vec = finder->eval(self);
		return !vec.empty() && std::all_of(vec.begin(), vec.end(), [this](ServerGameObject *obj) {return obj->blueprint->bpClass == objclass; });
	}
	ValueObjectClass(int objclass, ObjectFinder *finder) : objclass(objclass), finder(finder) {}
};

struct ValueEquationResult : ValueDeterminer {
	int equation;
	ObjectFinder *finder;
	float eval(ServerGameObject *self) {
		ServerGameObject *obj = finder->getFirst(self);
		return Server::instance->gameSet->equations[equation]->eval(obj);
	}
	ValueEquationResult(int equation, ObjectFinder *finder) : equation(equation), finder(finder) {}
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs)
{
	switch (Tags::VALUE_tagDict.getTagID(gsf.nextTag().c_str())) {
	case Tags::VALUE_CONSTANT:
		return new ValueConstant(gsf.nextFloat());
	case Tags::VALUE_DEFINED_VALUE:
		return new ValueConstant(gs.definedValues.at(gsf.nextString(true)));
	case Tags::VALUE_ITEM_VALUE: {
		int item = gs.items.readIndex(gsf);
		return new ValueItemValue(item, ReadFinder(gsf, gs));
	}
	case Tags::VALUE_OBJECT_ID:
		return new ValueObjectId_Battles(ReadFinder(gsf, gs));
	case Tags::VALUE_OBJECT_CLASS: {
		int objclass = Tags::GAMEOBJCLASS_tagDict.getTagID(gsf.nextString().c_str());
		return new ValueObjectClass(objclass, ReadFinder(gsf, gs));
	}
	case Tags::VALUE_EQUATION_RESULT: {
		int equation = gs.equations.readIndex(gsf);
		return new ValueEquationResult(equation, ReadFinder(gsf, gs));
	}
	default:
		return new ValueUnknown();
	}
}

struct EnodeAddition : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) { return a->eval(self) + b->eval(self); }
	EnodeAddition(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeNegate : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a;
	virtual float eval(ServerGameObject *self) { return -a->eval(self); }
	EnodeNegate(ValueDeterminer *a) : a(a) {}
};

struct EnodeSubtraction : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) { return a->eval(self) - b->eval(self); }
	EnodeSubtraction(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeMultiplication : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) { return a->eval(self) * b->eval(self); }
	EnodeMultiplication(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeDivision : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) { return a->eval(self) / b->eval(self); }
	EnodeDivision(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
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
			switch (Tags::ENODE_tagDict.getTagID(gsf.nextString().c_str())) {
			case Tags::ENODE_ADDITION:
				return new EnodeAddition(ReadEquationNode(gsf, gs), ReadEquationNode(gsf, gs));
			case Tags::ENODE_NEGATE:
				return new EnodeNegate(ReadEquationNode(gsf, gs));
			case Tags::ENODE_SUBTRACTION: {
				ValueDeterminer *a = ReadEquationNode(gsf, gs);
				return new EnodeSubtraction(a, ReadEquationNode(gsf, gs));
			}
			case Tags::ENODE_MULTIPLICATION: {
				ValueDeterminer *a = ReadEquationNode(gsf, gs);
				return new EnodeMultiplication(a, ReadEquationNode(gsf, gs));
			}
			case Tags::ENODE_DIVISION: {
				ValueDeterminer *a = ReadEquationNode(gsf, gs);
				return new EnodeDivision(a, ReadEquationNode(gsf, gs));
			}
			}
			gsf.cursor = oldcur;
			return ReadValueDeterminer(gsf, gs);
		}
		gsf.advanceLine();
	}
	ferr("Equation reached end of file without END_EQUATION!");
	return nullptr;
}

//}