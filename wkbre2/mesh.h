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
	std::array<float, 4> orientation;
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
	uint16_t numVertices, numNormals, numUvs, numAttachPoints;
	DynArray<AttachmentPoint> attachPoints;
	DynArray<float> vertices;
	Vector3 sphereCenter; float sphereRadius;
	DynArray<uint16_t> vertexRemapper;
	DynArray<Material> materials;
	DynArray<DynArray<float>> uvLists;
	DynArray<DynArray<IndexTuple>> groupIndices;
	DynArray<PolygonList> polyLists;
	DynArray<uint8_t> normals;
	DynArray<uint16_t> normalRemapper;

	void load(const char *filename);
	const Vector3* decodeNormals();

	Mesh() {}
	Mesh(const char *filename) { load(filename); }

	static const std::array<Vector3, 256> s_normalTable;
};