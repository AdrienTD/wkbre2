#pragma once

#include <string>
#include <map>
#include "renderer.h"

struct TextureCache
{
	IRenderer *gfx;
	std::map<std::string, texture> loadedTextures;
	std::string directory;

	texture loadTexture(const char *fn, int mipmaps);
	texture getTexture(const char *filename, bool mipmaps = true);

	TextureCache(IRenderer *gfx, const std::string &directory = "") : gfx(gfx), directory(directory) {}
};