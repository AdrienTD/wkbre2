// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "../util/vecmat.h"

struct ServerGameObject;
struct GSFileParser;
struct GameSet;
struct SrvScriptContext;
struct CliScriptContext;

struct OrientedPosition {
	Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 rotation = Vector3(0.0f, 0.0f, 0.0f); //float yRotation = 0.0f, xRotation = 0.0f;

	OrientedPosition operator+(const OrientedPosition &op) const { return { position + op.position, rotation + op.rotation }; }
	OrientedPosition &operator+=(const OrientedPosition &op) { position += op.position; rotation += op.rotation; return *this; }
	OrientedPosition operator/(float v) { return { position / v, rotation / v }; }

	OrientedPosition() = default;
	OrientedPosition(const Vector3& pos) : position(pos) {}
	OrientedPosition(const Vector3& pos, const Vector3& rot) : position(pos), rotation(rot) {}
	OrientedPosition(const Vector3& pos, float angle) : position(pos), rotation(Vector3(0.0f, angle, 0.0f)) {}
};

struct PositionDeterminer {
	using EvalRetSrv = OrientedPosition;
	//using EvalRetCli = OrientedPosition;
	virtual ~PositionDeterminer() {}
	virtual OrientedPosition eval(SrvScriptContext* ctx) = 0;
	virtual OrientedPosition eval(CliScriptContext* ctx) { return OrientedPosition(); }
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;

	static PositionDeterminer *createFrom(GSFileParser &gsf, GameSet &gs);
};