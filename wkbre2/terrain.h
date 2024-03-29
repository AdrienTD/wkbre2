// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "util/vecmat.h"
#include "TrnTextureDb.h"
#include "util/DynArray.h"

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

	int width, height, edge;
	float scale;
	uint32_t fogColor, sunColor;
	Vector3 sunVector;
	std::wstring texDbPath, skyTextureDirectory, filePath;
	std::vector<Vector3> lakes;

	TerrainTextureDatabase texDb;
	DynArray<uint8_t> vertices;
	Tile *tiles;

	std::pair<int, int> getNumPlayableTiles() const { return { width - 2 * edge, height - 2 * edge }; }
	std::pair<float, float> getPlayableArea() const { return { 5.0f * (width - 2 * edge), 5.0f * (height - 2 * edge) }; }

	inline float getVertex(int x, int z) const
	{
		return vertices[z*(width+1) + x] * scale;
	}

	float getHeight(float x, float z) const;
	float getHeightEx(float x, float z, bool floatOnWater) const;

	Vector3 getNormal(int x, int z) const;

	const Tile* getTile(int tx, int tz) const { return (tx >= 0 && tx < width && tz >= 0 && tz < height) ? &tiles[(height-1-tz) * width + tx] : nullptr; }
	const Tile* getPlayableTile(int tx, int tz) const { return getTile(tx + edge, tz + edge); }

	void freeArrays() {
		vertices.resize(0);
		if (tiles) delete[] tiles;
	}

	void createEmpty(int newWidth, int newHeight);

	static int GetMaxBits(int x);

	void readFromFile(const char* filename);
	void readBCM(const char *filename);
	void readSNR(const char* filename);
	void readTRN(const char* filename);

	void floodfillWater();
};
		