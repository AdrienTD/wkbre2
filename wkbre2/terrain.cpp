// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "terrain.h"
#include "file.h"
#include "Camera.h"
#include "util/util.h"
#include "util/GSFileParser.h"
#include "util/StriCompare.h"
#include "gfx/bitmap.h"
#include <algorithm>
#include <cstring>

float Terrain::getHeight(float ipx, float ipy) const {
	//int tx = floorf(x / 5) + edge, tz = floorf(z / 5) + edge;
	//if (tx < 0 || tx >= width || tz < 0 || tz >= height)
	//	return 0.0f;
	//return getVertex(tx, tz);
	float x = edge + ipx / 5.0f, y = edge + ipy / 5.0f;
	if (x < 0) x = 0; if (x >= width) x = width - 0.001f;
	if (y < 0) y = 0; if (y >= height) y = height - 0.001f;
	float h1 = getVertex((int)x, (int)y), h2 = getVertex((int)x + 1, (int)y);
	float h3 = getVertex((int)x, (int)y + 1), h4 = getVertex((int)x + 1, (int)y + 1);
	int rx = (int)x; int ry = (int)y;
	float qx = x - rx, qy = y - ry;
	float h5 = h1 * (1.0f - qx) + h2 * qx;
	float h6 = h3 * (1.0f - qx) + h4 * qx;
	float hr = h5 * (1.0f - qy) + h6 * qy;
	return hr;
}

float Terrain::getHeightEx(float x, float z, bool floatOnWater) const
{
	float h = getHeight(x, z);
	if (floatOnWater) {
		int ix = (int)(edge + x / 5.0f);
		int iz = (int)(edge + z / 5.0f);
		if (ix >= 0 && ix < width && iz >= 0 && iz < height) {
			if (const Tile* tile = getTile(ix, iz)) {
				if (tile->fullOfWater && tile->waterLevel > h)
					h = tile->waterLevel;
			}
		}
	}
	return h;
}

Vector3 Terrain::getNormal(int x, int z) const {
	//Vector3 nrm(0,0,0);
	//float h = getVertex(x, z);
	//if (x > 0 && z > 0)
	//	nrm += Vector3(h - getVertex(x - 1, z), 1, h - getVertex(x, z - 1)).normal();
	//if (x < width && z > 0)
	//	nrm += Vector3(h - getVertex(x + 1, z), 1, h - getVertex(x, z - 1)).normal();
	//if (x < width && z < height)
	//	nrm += Vector3(h - getVertex(x + 1, z), 1, h - getVertex(x, z + 1)).normal();
	//if (x > 0 && z < height)
	//	nrm += Vector3(h - getVertex(x - 1, z), 1, h - getVertex(x, z + 1)).normal();
	//return nrm.normal();
	Vector3 nrm(0, 0, 0);
	float h = getVertex(x, z);

	float a, b, c, d;
	if (z > 0)		a = getVertex(x, z - 1) - h;
	if (x > 0)		b = getVertex(x - 1, z) - h;
	if (z < height)	c = getVertex(x, z + 1) - h;
	if (x < width)	d = getVertex(x + 1, z) - h;

	if (x > 0 && z > 0)				nrm += Vector3(b , 5,  a);
	if (x > 0 && z < height)		nrm += Vector3(b , 5, -c);
	if (x < width && z < height)	nrm += Vector3(-d, 5, -c);
	if (x < width && z > 0)			nrm += Vector3(-d, 5,  a);
	return nrm.normal();

}

void Terrain::createEmpty(int newWidth, int newHeight) {
	width = newWidth; height = newHeight; edge = 0;
	scale = 1.0f;
	fogColor = 0x9FC5E6;
	sunColor = -1;
	sunVector = Vector3(1, 1, 1);
}

int Terrain::GetMaxBits(int x)
{
	for (int i = 1; i < 32; i++)
		if ((1 << i) > x) // > sign (not >=) intentional!
			return i;
	//assert(0 & (int)"no max bits found");
	return 0;
}

void Terrain::readFromFile(const char* filename)
{
	const char* ext = strrchr(filename, '.');
	if (!StrCICompare(ext, ".bcm"))
		readBCM(filename);
	else
		readSNR(filename);
}

void Terrain::readBCM(const char * filename) {
	char *fcnt, *fp; int fsize;

	LoadFile(filename, &fcnt, &fsize);
	fp = fcnt + 12;
	width = *(uint32_t*)fp; fp += 4;
	height = *(uint32_t*)fp; fp += 4;
	edge = *(uint32_t*)fp; fp += 4;
	uint16_t strs1 = *(uint16_t*)fp / 2; fp += 2;
	texDbPath = std::wstring((wchar_t*)fp, strs1);  fp += strs1 * 2;
	scale = *(float*)fp; fp += 4;
	sunColor = (*(uint8_t*)fp << 16) | ((*(uint8_t*)(fp + 4)) << 8) | (*(uint8_t*)(fp + 8)); fp += 12;
	sunVector.x = *(float*)fp; sunVector.y = *(float*)(fp + 4); sunVector.z = *(float*)(fp + 8); fp += 12;
	fogColor = (*(uint8_t*)fp << 16) | ((*(uint8_t*)(fp + 4)) << 8) | (*(uint8_t*)(fp + 8)); fp += 12;
	uint16_t strs2 = *(uint16_t*)fp / 2; fp += 2;
	skyTextureDirectory = std::wstring((wchar_t*)fp, strs2); fp += strs2 * 2;

	std::string ctexDbPath((uint16_t*)texDbPath.data(), (uint16_t*)texDbPath.data()+strs1);
	texDb.load(ctexDbPath.c_str());
	texDb.translate(texDb.getLTTTPath(filename).c_str());

	int numVerts = (width + 1)*(height + 1);
	vertices.resize(numVerts);

	BitReader br(fp + 0xC000);

	// Lakes
	int mapnlakes = br.readnb(6);
	for (int i = 0; i < mapnlakes; i++)
	{
		Vector3 v; int j;
		j = br.readnb(32); v.x = *(float*)&j;
		j = br.readnb(32); v.y = *(float*)&j;
		j = br.readnb(32); v.z = *(float*)&j;
		br.readnb(2);
		lakes.push_back(std::move(v));
	}

	// Group name table
	int nnames = br.readnb(16);
	TerrainTextureGroup **grptable = new TerrainTextureGroup*[nnames];
	for (int i = 0; i < nnames; i++)
	{
		int n = br.readnb(8);
		char *a = new char[n + 1];
		for (int j = 0; j < n; j++) a[j] = br.readnb(8);
		a[n] = 0;
		grptable[i] = texDb.groups[a].get();
		if(grptable[i] == nullptr)
			ferr("BCM file uses unknown texture group.");
		delete[] a;
	}

	// ID table
	int nids = br.readnb(16);
	int *idtable = new int[nids];
	for (int i = 0; i < nids; i++)
		idtable[i] = br.readnb(32);

	// Tiles
	tiles = new Tile[width*height];
	Tile *t = tiles;
	int ngrpbits = GetMaxBits(nnames);
	int nidbits = GetMaxBits(nids);
	for (int z = height - 1; z >= 0; z--) {
		t = &tiles[z*width];
		for (int x = 0; x < width; x++)
		{
			int tg = br.readnb(ngrpbits); int ti = br.readnb(nidbits);
			int tr = br.readnb(2); int tx = br.readbit(); int tz = br.readbit();
			TerrainTextureGroup *g = grptable[tg];
			int id = idtable[ti];
			TerrainTexture *tex = g->textures[id].get();
			t->texture = tex;
			t->rot = tr; t->xflip = tx; t->zflip = tz;
			t->x = x; t->z = z;
			t++;
		}
	}
	//delete[] grptable; delete[] idtable;

	// ?
	int nunk = br.readnb(32);
	br.readnb(32); // this should return 0x62
	for (int i = 0; i < nunk; i++)
	{
		br.readnb(ngrpbits); br.readnb(nidbits);
	}

	// Heightmap
	for (int y = 0; y <= height; y++)
		for (int x = 0; x <= width; x++)
			vertices[(height - y) * (width + 1) + x] = (uint8_t)br.readnb(8);

	free(fcnt);

	floodfillWater();
	updateQuadTree();
}

void Terrain::readSNR(const char* filename)
{
	char* text; int textsize;
	LoadFile(filename, &text, &textsize, 1);
	text[textsize] = 0;

	GSFileParser gsf(text);
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "SCENARIO_DIMENSIONS") {
			width = gsf.nextInt();
			height = gsf.nextInt();
		}
		else if (tag == "SCENARIO_EDGE_WIDTH")
			edge = gsf.nextInt();
		else if (tag == "SCENARIO_TEXTURE_DATABASE") {
			auto str = gsf.nextString(true);
			texDbPath = std::wstring(str.begin(), str.end());
			texDb.load(str.c_str());
		}
		else if (tag == "SCENARIO_TERRAIN") {
			auto str = gsf.nextString(true);
			readTRN(str.c_str());
			texDb.translate(texDb.getLTTTPath(str).c_str());
		}
		else if (tag == "SCENARIO_HEIGHTMAP") {
			int numVerts = (width + 1) * (height + 1);
			Bitmap bmp = Bitmap::loadBitmap(gsf.nextString(true).c_str());
			if (bmp.width != width + 1 || bmp.height != height + 1)
				ferr("Incorrect heightmap bitmap size");
			vertices.resize(numVerts);
			for (int y = 0; y <= height; ++y)
				memcpy(vertices.data() + (height - y) * (width + 1), bmp.pixels.data() + y * (width + 1), width + 1);
		}
		else if (tag == "SCENARIO_HEIGHT_SCALE_FACTOR") {
			scale = gsf.nextFloat();
		}
		else if (tag == "SCENARIO_SUN_COLOUR") {
			sunColor = 0;
			for (int i = 0; i < 3; i++)
				sunColor = (sunColor << 8) | gsf.nextInt();
		}
		else if (tag == "SCENARIO_SUN_VECTOR") {
			for (float& f : sunVector)
				f = gsf.nextFloat();
		}
		else if (tag == "SCENARIO_FOG_COLOUR") {
			fogColor = 0;
			for (int i = 0; i < 3; i++)
				fogColor = (fogColor << 8) | gsf.nextInt();
		}
		else if (tag == "SCENARIO_SKY_TEXTURES_DIRECTORY") {
			auto str = gsf.nextString(true);
			skyTextureDirectory = std::wstring(str.begin(), str.end());
		}
		else if (tag == "SCENARIO_MINIMAP") {
		}
		else if (tag == "SCENARIO_LAKE") {
			Vector3 vec;
			for (float& f : vec) f = gsf.nextFloat();
			lakes.push_back(vec);
		}

		gsf.advanceLine();
	}

	free(text);

	floodfillWater();
	updateQuadTree();
}

void Terrain::readTRN(const char* filename)
{
	tiles = new Tile[width * height];

	char* text; int textsize;
	LoadFile(filename, &text, &textsize, 1);
	text[textsize] = 0;

	GSFileParser gsf(text);
	while (!gsf.eof) {
		std::string str;

		str = gsf.nextString();
		if (str == "X") {
			int x = gsf.nextInt() - 1;

			str = gsf.nextString();
			int z = gsf.nextInt() - 1;

			str = gsf.nextString();
			auto grp = gsf.nextString(true);

			str = gsf.nextString();
			int id = gsf.nextInt();

			str = gsf.nextString();
			int rot = gsf.nextInt();

			str = gsf.nextString();
			bool xflip = gsf.nextInt();

			str = gsf.nextString();
			bool zflip = gsf.nextInt();

			Tile& t = tiles[width * z + x];
			t.x = x; t.z = z;
			t.rot = rot; t.xflip = xflip; t.zflip = zflip;
			t.texture = texDb.groups.at(grp)->textures.at(id).get();
		}
		gsf.advanceLine();
	}

	free(text);
}

void Terrain::floodfillWater()
{
	auto fillTile = [this](int tx, int tz, float lvl) -> bool {
		Tile& tile = tiles[tz * width + tx];
		if (tile.fullOfWater && lvl <= tile.waterLevel)
			return false;

		float h00 = getVertex(tx, tz);
		float h01 = getVertex(tx, tz+1);
		float h10 = getVertex(tx+1, tz);
		float h11 = getVertex(tx+1, tz+1);
		if (std::min({ h00,h01,h10,h11 }) < lvl) {
			tile.waterLevel = lvl;
			tile.fullOfWater = true;
			return true;
		}
		return false;
	};
	auto floodfill = [this, &fillTile](int tx, int tz, float lvl, const auto& rec) -> void {
		bool done = fillTile(tx, tz, lvl);
		if (!done) return;
		int x, xmin, xmax;
		for (x = tx - 1; x >= 0; x--) {
			bool done = fillTile(x, tz, lvl);
			if (!done) break;
		}
		xmin = x+1;
		for (x = tx + 1; x < width; x++) {
			bool done = fillTile(x, tz, lvl);
			if (!done) break;
		}
		xmax = x-1;
		for (x = xmin; x <= xmax; x++) {
			if (tz - 1 >= 0)
				rec(x, tz - 1, lvl, rec);
			if (tz + 1 < height)
				rec(x, tz + 1, lvl, rec);
		}
	};
	for (Vector3& lake : lakes) {
		floodfill((int)(lake.x / 5.0f), (int)(lake.z / 5.0f), lake.y, floodfill);
	}
}

void Terrain::updateQuadTree()
{
	std::vector<TerrainQuadNode> quadTree;

	auto generateQuadNode = [&quadTree, terrain = this](int start_x, int start_z, int len_x, int len_z, const auto& rec) -> int {
		if (len_x == 0 || len_z == 0) {
			return -1;
		}

		const int index = quadTree.size();
		quadTree.emplace_back();

		TerrainQuadNode node;

		if (len_x == 1 && len_z == 1)
		{
			const float h0 = terrain->getVertex(start_x, start_z);
			const float h1 = terrain->getVertex(start_x + 1, start_z);
			const float h2 = terrain->getVertex(start_x, start_z + 1);
			const float h3 = terrain->getVertex(start_x + 1, start_z + 1);
			std::tie(node.minHeight, node.maxHeight) = std::minmax({ h0, h1, h2, h3 });

			const auto& tile = terrain->getTile(start_x, start_z);
			if (tile->fullOfWater)
				node.maxHeight = std::max(node.maxHeight, tile->waterLevel);
		}
		else
		{
			const int first_half_x = len_x / 2;
			const int first_half_z = len_z / 2;
			const int second_half_x = len_x - first_half_x;
			const int second_half_z = len_z - first_half_z;
			node.topLeft = rec(start_x, start_z, first_half_x, first_half_z, rec);
			node.topRight = rec(start_x + first_half_x, start_z, second_half_x, first_half_z, rec);
			node.bottomLeft = rec(start_x, start_z + first_half_z, first_half_x, second_half_z, rec);
			node.bottomRight = rec(start_x + first_half_x, start_z + first_half_z, second_half_x, second_half_z, rec);

			node.minHeight = std::numeric_limits<float>::max();
			node.maxHeight = std::numeric_limits<float>::lowest();
			for (int n : {node.topLeft, node.topRight, node.bottomLeft, node.bottomRight}) {
				if (n != -1) {
					node.minHeight = std::min(node.minHeight, quadTree[n].minHeight);
					node.maxHeight = std::max(node.maxHeight, quadTree[n].maxHeight);
				}
			}
		}

		quadTree[index] = node;

		return index;
		};

	generateQuadNode(0, 0, this->width, this->height, generateQuadNode);
	this->quadTree = std::move(quadTree);
}

int Terrain::iterateVisibleTiles(const Camera& camera, const std::function<void(int, int, int, int)>& func)
{
	int statQuadNodesIterated = 0;

	const auto iterQuadNode = [this, &statQuadNodesIterated, &camera, &func](int index, int start_x, int start_z, int len_x, int len_z, const auto& rec) -> void {
		statQuadNodesIterated += 1;

		if (index == -1 || len_x == 0 || len_z == 0)
			return;
		const auto& quadNode = this->quadTree[index];

		bool drawNow = len_x <= 2 && len_z <= 2;

		if (!drawNow) {
			const auto t = [this](int x) {return (x - this->edge) * 5.0f; };
			const float x0 = t(start_x), x1 = t(start_x + len_x);
			const float z0 = t(start_z), z1 = t(start_z + len_z);
			const auto visibility = camera.isAABBVisible(Vector3(x0, quadNode.minHeight, z0), Vector3(x1, quadNode.maxHeight, z1));
			if (visibility == Camera::Visibility::Invisible)
			{
				// box completely outside the frustum -> skip quad node entirely
				return;
			}
			drawNow = visibility == Camera::Visibility::CompletelyVisible;
		}


		if (drawNow) {
			// box completely inside the frustum -> draw all the tiles directly
			func(start_x, start_z, len_x, len_z);
		}
		else {
			// box partially visible or uncertain -> continue walking through the quad nodes
			const int first_half_x = len_x / 2;
			const int first_half_z = len_z / 2;
			const int second_half_x = len_x - first_half_x;
			const int second_half_z = len_z - first_half_z;
			rec(quadNode.topLeft, start_x, start_z, first_half_x, first_half_z, rec);
			rec(quadNode.topRight, start_x + first_half_x, start_z, second_half_x, first_half_z, rec);
			rec(quadNode.bottomLeft, start_x, start_z + first_half_z, first_half_x, second_half_z, rec);
			rec(quadNode.bottomRight, start_x + first_half_x, start_z + first_half_z, second_half_x, second_half_z, rec);
		}
		};
	iterQuadNode(0, 0, 0, this->width, this->height, iterQuadNode);

	return statQuadNodesIterated;
}
