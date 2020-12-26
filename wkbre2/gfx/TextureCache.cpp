#include "TextureCache.h"
#include "../file.h"
#include "renderer.h"
#include "bitmap.h"

texture TextureCache::loadTexture(const char * fn, int mipmaps)
{
	Bitmap *bm;
	if (FileExists(fn))
		bm = LoadBitmap(fn);
	else
		bm = LoadBitmap("netexfb.tga");
	texture t = gfx->CreateTexture(bm, mipmaps);
	FreeBitmap(bm);
	return t;
}

texture TextureCache::getTexture(const char * filename, bool mipmaps) {
	auto it = loadedTextures.find(filename);
	if (it != loadedTextures.end())
		return it->second;
	texture tex = loadTexture((directory + filename).c_str(), mipmaps ? 0 : 1); // todo
	loadedTextures[filename] = tex;
	return tex;
}