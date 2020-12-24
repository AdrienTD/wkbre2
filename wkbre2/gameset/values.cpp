
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
		return AnyGameObject::Program::instance->gameSet->equations[equation]->eval(obj);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	//ValueEquationResult() {}
	//ValueEquationResult(int equation, ObjectFinder *finder) : equation(equation), finder(finder) {}
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
	default: vd = new ValueUnknown; break;
	}
	vd->parse(gsf, const_cast<GameSet&>(gs));
	return vd;
}

struct EnodeAddition : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) override { return a->eval(self) + b->eval(self); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
	EnodeAddition() {}
	EnodeAddition(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeNegate : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a;
	virtual float eval(ServerGameObject *self) override { return -a->eval(self); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
	}
	EnodeNegate() {}
	EnodeNegate(ValueDeterminer *a) : a(a) {}
};

struct EnodeSubtraction : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) override { return a->eval(self) - b->eval(self); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
	EnodeSubtraction() {}
	EnodeSubtraction(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeMultiplication : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) override { return a->eval(self) * b->eval(self); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
	EnodeMultiplication() {}
	EnodeMultiplication(ValueDeterminer *a, ValueDeterminer *b) : a(a), b(b) {}
};

struct EnodeDivision : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual float eval(ServerGameObject *self) override { return a->eval(self) / b->eval(self); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
	EnodeDivision() {}
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
			ValueDeterminer *vd;
			switch (Tags::ENODE_tagDict.getTagID(gsf.nextString().c_str())) {
			case Tags::ENODE_ADDITION: vd = new EnodeAddition; break;
			case Tags::ENODE_NEGATE: vd = new EnodeNegate; break;
			case Tags::ENODE_SUBTRACTION: vd = new EnodeSubtraction; break;
			case Tags::ENODE_MULTIPLICATION: vd = new EnodeMultiplication; break;
			case Tags::ENODE_DIVISION: vd = new EnodeDivision; break;
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