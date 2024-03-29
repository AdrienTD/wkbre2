// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "TextureCache.h"
#include "../file.h"
#include "renderer.h"
#include "bitmap.h"

texture TextureCache::loadTexture(const char * fn, int mipmaps)
{
	Bitmap bm;
	if (FileExists(fn))
		bm = Bitmap::loadBitmap(fn);
	else
		return nullptr; //bm = LoadBitmap("netexfb.tga");
	texture t = gfx->CreateTexture(bm, mipmaps);
	return t;
}

texture TextureCache::createTexture(const char* filename, int mipmaps, const Bitmap& bmp)
{
	texture t = gfx->CreateTexture(bmp, mipmaps);
	loadedTextures[filename] = t;
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

texture TextureCache::getTextureIfCached(const char* filename) const
{
	auto it = loadedTextures.find(filename);
	if (it != loadedTextures.end())
		return it->second;
	else
		return nullptr;
}
