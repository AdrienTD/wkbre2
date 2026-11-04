// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>
#include <string>
#include <utility>
#include <map>
#include <memory>
#include "util/IndexedStringList.h"

struct TerrainTextureGroup;
struct TerrainTexture;

struct TerrainTextureEdge
{
	TerrainTexture *tex;
	unsigned int rot : 2, xflip : 1, zflip : 1;
	// NOTE: Using memcmp with "boolean" types is dangerous!
	bool operator==(TerrainTextureEdge &b) { return tex == b.tex && rot == b.rot && xflip == b.xflip && zflip == b.zflip; }
};

struct TerrainTexture
{
	int id;
	int startx, starty, width, height;
	int texFilenameId;
	TerrainTextureGroup *grp;
	std::vector<TerrainTextureEdge> edges[4];
};

struct TerrainTextureGroup
{
	std::string name;
	std::map<int, std::unique_ptr<TerrainTexture>> textures;
};

struct TerrainTextureDatabase
{
	std::map<std::string, std::unique_ptr<TerrainTextureGroup>> groups;
	IndexedStringList textureFilenames;

	void load(const char *filename);
	void translate(const char* ltttFilePath);
	TerrainTextureDatabase() {}
	TerrainTextureDatabase(const char *filename) { load(filename); }

	static std::string getLTTTPath(const std::string& terrainFilePath);
};
