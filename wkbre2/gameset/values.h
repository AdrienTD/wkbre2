#pragma once

struct GSFileParser;
struct GameSet;
struct ServerGameObject;

//namespace Script {

struct ValueDeterminer {
	virtual float eval(ServerGameObject *self) = 0;
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs);
ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs);

//}