#include "terrain.h"
#include "file.h"

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

	if (x > 0 && z > 0)				nrm += Vector3(b , 5, -a);
	if (x > 0 && z < height)		nrm += Vector3(b , 5,  c);
	if (x < width && z < height)	nrm += Vector3(-d, 5,  c);
	if (x < width && z > 0)			nrm += Vector3(-d, 5, -a);
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

void Terrain::readBCM(const char * filename) {
	char *fcnt, *fp; int fsize, strs;

	LoadFile(filename, &fcnt, &fsize);
	fp = fcnt + 12;
	width = *(uint32_t*)fp; fp += 4;
	height = *(uint32_t*)fp; fp += 4;
	edge = *(uint32_t*)fp; fp += 4;
	strs = *(uint16_t*)fp / 2; fp += 2;
	texDbPath = std::wstring((wchar_t*)fp, strs);  fp += strs * 2;
	scale = *(float*)fp; fp += 4;
	sunColor = (*(uint8_t*)fp << 16) | ((*(uint8_t*)(fp + 4)) << 8) | (*(uint8_t*)(fp + 8)); fp += 12;
	sunVector.x = *(float*)fp; sunVector.y = *(float*)(fp + 4); sunVector.z = *(float*)(fp + 8); fp += 12;
	fogColor = (*(uint8_t*)fp << 16) | ((*(uint8_t*)(fp + 4)) << 8) | (*(uint8_t*)(fp + 8)); fp += 12;
	strs = *(uint16_t*)fp / 2; fp += 2;
	skyTextureDirectory = std::wstring((wchar_t*)fp, strs); fp += strs * 2;

	std::string ctexDbPath(texDbPath.begin(), texDbPath.end());
	texDb.load(ctexDbPath.c_str());

	int numVerts = (width + 1)*(height + 1);
	vertices = (uint8_t*)malloc(numVerts);
	if (!vertices) ferr("No mem. left for heightmap.");

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
	//for (int z = height - 1; z >= 0; z--) {
	for (int z = 0 ; z < height; z++) {
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
	for (int i = 0; i < numVerts; i++)
		vertices[i] = (uint8_t)br.readnb(8);

	free(fcnt);
}
