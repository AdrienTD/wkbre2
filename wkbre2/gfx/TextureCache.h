// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <map>
#include "renderer.h"

struct Bitmap;

struct TextureCache
{
	IRenderer *gfx;
	std::map<std::string, texture> loadedTextures;
	std::string directory;

	texture loadTexture(const char *fn, int mipmaps);
	texture createTexture(const char* filename, int mipmaps, const Bitmap &bmp);

	texture getTexture(const char *filename, bool mipmaps = true);
	texture getTextureIfCached(const char* filename) const;

	TextureCache(IRenderer *gfx, const std::string &directory = "") : gfx(gfx), directory(directory) {}
};