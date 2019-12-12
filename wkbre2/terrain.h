#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "util/vecmat.h"
#include "TrnTextureDb.h"

struct BitReader
{
	uint8_t *bytepos;
	unsigned int bitpos;

	int readbit()
	{
		int a;
		if(bitpos >= 8) {bitpos = 0; bytepos++;}
		a = ((*bytepos) & (1<<bitpos))?1:0;
		bitpos++;
		return a;
	}
	int readnb(int b)
	{
		int a = 0;
		for(int i = 0; i < b; i++)
			a |= readbit() << i;
		return a;
	}

	BitReader(void *data) : bytepos((uint8_t*)data), bitpos(0) {}
};

struct TerrainTexture;

struct TerrainTile {
	TerrainTexture *texture;
	unsigned int rot : 2, xflip : 1, zflip : 1;
	unsigned int x, z;
	float waterLevel; bool fullOfWater;
	unsigned int lakeId; // when removing lake, do a full autofill again.
};

struct Terrain
{
	typedef TerrainTile Tile;

	unsigned int width, height, edge;
	float scale;
	uint32_t fogColor, sunColor;
	Vector3 sunVector;
	std::wstring texDbPath, skyTextureDirectory, filePath;
	std::vector<Vector3> lakes;

	TerrainTextureDatabase texDb;
	uint8_t *vertices;
	Tile *tiles;

	inline float getVertex(int x, int z) const
	{
		return vertices[z*(width+1) + x] * scale;
	}

	Vector3 getNormal(int x, int z) const;

	void freeArrays() {
		if (vertices) delete[] vertices;
		if (tiles) delete[] tiles;
	}

	void createEmpty(int newWidth, int newHeight);

	static int GetMaxBits(int x);

	void readBCM(const char *filename);
};
		