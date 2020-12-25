#pragma once

#include "../util/vecmat.h"

struct ServerGameObject;
struct GSFileParser;
struct GameSet;

struct OrientedPosition {
	Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
	float yRotation = 0.0f, xRotation = 0.0f;

	OrientedPosition operator+(const OrientedPosition &op) const { return { position + op.position, yRotation + op.yRotation, xRotation + op.xRotation }; }
	OrientedPosition &operator+=(const OrientedPosition &op) { position += op.position; yRotation += op.yRotation; xRotation += op.xRotation; return *this; }
	OrientedPosition operator/(float v) { return { position / v, yRotation / v, xRotation / v }; }
};

struct PositionDeterminer {
	using EvalRetSrv = OrientedPosition;
	//using EvalRetCli = OrientedPosition;
	virtual ~PositionDeterminer() {}
	virtual OrientedPosition eval(ServerGameObject *self) = 0;
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;

	static PositionDeterminer *createFrom(GSFileParser &gsf, GameSet &gs);
};