#pragma once

#include "util/vecmat.h"
#include <string>
#include <utility>
#include <cstdint>
#include <array>
#include "util/DynArray.h"

struct AttachmentPointState
{
	Vector3 position;
	float orientation[4];
	bool on;
};

struct AttachmentPoint
{
	std::string tag;
	AttachmentPointState staticState;
	std::string path;
	//Model *model;
};

struct Material
{
	std::string textureFilename;
	bool alphaTest;
};

struct IndexTuple
{
	uint16_t vertex, normal, uv;
};

struct PolyListGroup {
	DynArray<std::array<uint16_t, 3>> tupleIndex;
};

struct PolygonList
{
	uint16_t numVerts, numNorms, numUvs;
	float distance;
	DynArray<PolyListGroup> groups;
};

struct Mesh
{
	uint16_t numVertices, numUvs, numAttachPoints;
	DynArray<AttachmentPoint> attachPoints;
	DynArray<float> vertices;
	float sphere[4];
	DynArray<uint16_t> vertexRemapper;
	DynArray<Material> materials;
	DynArray<DynArray<float>> uvLists;
	DynArray<DynArray<IndexTuple>> groupIndices;
	DynArray<PolygonList> polyLists;

	void load(const char *filename);

	Mesh() {}
	Mesh(const char *filename) { load(filename); }
};