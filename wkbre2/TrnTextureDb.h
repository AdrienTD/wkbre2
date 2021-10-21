#pragma once

#include <vector>
#include <string>
#include <utility>
#include <map>
#include <memory>

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
	//texture t;
	//int tfid;
	std::string file;
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
	std::vector<std::string> textureFilenames;

	void load(const char *filename);
	void translate(const char* ltttFilePath);
	void loadAndTranslate(const char* filename, const std::string& terrainFilePath);
	TerrainTextureDatabase() {}
	TerrainTextureDatabase(const char *filename) { load(filename); }
};

void FreeLTTT();
void FreeTerrainTextures();
void LoadLTTT(char *fn);
void LoadTerrainTextures();
