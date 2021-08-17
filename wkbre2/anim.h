#pragma once

#include <cstdint>
#include <string>
#include <array>
#include "mesh.h"

//struct AttachmentPointState;
//struct Mesh;

struct Anim
{
	struct AnimPosCoord
	{
		uint32_t numFrames;
		DynArray<uint32_t> frameTimes;
		DynArray<DynArray<uint32_t>> verts;
		DynArray<float> vertadd, vertmul;
	};

	struct AnimAttachPoint
	{
		uint32_t numFrames;
		DynArray<uint32_t> frameTimes;
		DynArray<AttachmentPointState> states;
		size_t getAPFrame(uint32_t animTime);
	};

	//Mesh *mesh;
	std::string meshname;
	uint32_t duration;
	uint32_t numVertices;
	std::array<AnimPosCoord,3> coords;
	//uint32_t numAttachPoints;
	DynArray<AnimAttachPoint> attachPoints;

	void load(const char *filename);
	const float* interpolate(uint32_t animTime, const Mesh& mesh);
	AttachmentPointState getAPState(size_t index, uint32_t animTime);

	Anim() {}
	Anim(const char *filename) { load(filename); }
};
