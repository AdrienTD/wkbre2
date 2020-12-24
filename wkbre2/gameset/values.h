#pragma once

struct GSFileParser;
struct GameSet;
struct ServerGameObject;
struct ClientGameObject;

//namespace Script {

struct ValueDeterminer {
	using EvalRetSrv = float;
	using EvalRetCli = float;
	virtual ~ValueDeterminer() {}
	virtual float eval(ServerGameObject *self) = 0;
	virtual float eval(ClientGameObject *self);
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs);
ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs);

//}