// wkbre - WK (Battles) recreated game engine
// Copyright (C) 2015-2016 Adrien Geets
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "bitmap.h"
#include "../file.h"
#include "../util/util.h"
#include <cstdint>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_BOX
#include <stb_image_resize.h>

Bitmap *LoadBitmap(const char *fn)
{
	char *d; int s;
	const char *e = strrchr(fn, '.');
	LoadFile(fn, &d, &s);
	if(!_stricmp(e, ".tga"))
		return LoadTGA(d, s);
	if(!_stricmp(e, ".pcx"))
		return LoadPCX(d, s);
	ferr("Unknown bitmap file extension."); return 0;
}

Bitmap *LoadTGA(const char *data, int ds)
{
	Bitmap *bm = new Bitmap;
	bm->width = *(uint16_t*)(data+12);
	bm->height = *(uint16_t*)(data+14);
	if(data[1] && ((data[2]&7) == 1) && (data[7] == 24) && (data[16] == 8))
		bm->format = BMFORMAT_P8;
	else if(((data[2]&7) == 2) && (data[16] == 24))
		bm->format = BMFORMAT_B8G8R8;
	else if(((data[2]&7) == 2) && (data[16] == 32))
		bm->format = BMFORMAT_B8G8R8A8;
	else
		ferr("Unknown TGA image format.");
	const char *dp = data + 18 + (uint8_t)data[0];

	// 24-bits palette: BB GG RR    (!)
	// 24-bits bitmap:  BB GG RR    (!)
	// 32-bits bitmap:  BB GG RR AA (!)
	if(data[1])
	{
		bm->palette = (uint8_t*)malloc(768);
		for(int i = 0; i < 256; i++)
		{
			bm->palette[i*3+0] = dp[i*3+2];
			bm->palette[i*3+1] = dp[i*3+1];
			bm->palette[i*3+2] = dp[i*3+0];
		}
		dp += 768;
	} else
		bm->palette = 0;

	int e = data[16] / 8;
	e = ((data[16]&7)?(e+1):e);
	//printf("e = %i\n", e);
	int s = bm->width * bm->height * e;
	bm->pixels = (uint8_t*)malloc(s);
	//memcpy(bm->pixels, dp, s);
	if(data[2] & 8) // RLE
	{
		int x = 0, y = (data[17]&32) ? 0 : (bm->height - 1);
		const char *c = dp;
		while((y >= 0) || (y < bm->height))
		{
			uint8_t ua = *(c++);
			uint32_t us = ((uint8_t)ua & 127) + 1;

			for(int i = 0; i < us; i++)
			{
			 	for(int j = 0; j < e; j++)	
					bm->pixels[(y * bm->width + x) * e + j] = c[j];
				x++;
				if(!(ua & 128)) c += e;
				if(x >= bm->width)
				{
					x = 0;
					y += (data[17]&32) ? 1 : (-1);
					if((y < 0) || (y >= bm->height)) goto rleend;
				}
			}
			if(ua & 128) c += e;
		}
	rleend:	;
	}
	else
	{
		if(data[17] & 32)
			for(int y = 0; y < bm->height; y++)
				memcpy(bm->pixels + (y * bm->width * e), dp + (y * bm->width * e), bm->width * e);
		else
			for(int y = 0; y < bm->height; y++)
				memcpy(bm->pixels + ((bm->height-1-y) * bm->width * e), dp + (y * bm->width * e), bm->width * e);
	}

	return bm;
}

Bitmap *LoadPCX(const char *data, int ds)
{
	Bitmap *bm = new Bitmap;
	int nplanes = (uint8_t)data[65];
	if((uint8_t)data[3] != 8) ferr("PCX files that don't have 8 bits per channel are not supported.");
	bm->width = *((uint16_t*)&(data[8])) - *((uint16_t*)&(data[4])) + 1;
	bm->height = *((uint16_t*)&(data[10])) - *((uint16_t*)&(data[6])) + 1;
	bm->format = (nplanes==3) ? BMFORMAT_R8G8B8 : BMFORMAT_P8;
	bm->pixels = (uint8_t*)malloc(bm->width * bm->height * nplanes);
	int pitch = (bm->width & 1) ? (bm->width + 1) : bm->width;

	uint8_t *f = (uint8_t*)data + 128, *o = bm->pixels; int pp = 0;
	while(pp < (pitch * bm->height * nplanes))
	{
		int a = *f++, c;
		if(a >= 192)
			{c = a & 63; a = *f++;}
		else	c = 1;
		for(; c > 0; c--)
			{if(bm->width & 1)
				if( (pp % (bm->width+1)) == bm->width)	
					{pp++; continue;}
			if(bm->format == BMFORMAT_P8)
			{
				*o++ = a;
			/*
				int oy = pp / pitch;
				int ox = pp % pitch;
				bm->pixels[oy*bm->width + ox] = a;
			*/
			}
			else if(bm->format == BMFORMAT_R8G8B8)
			{
				int pl = (pp / pitch) % 3;
				int oy = (pp / pitch) / 3;
				int ox = pp % pitch;
				bm->pixels[(oy*bm->width+ox)*3+pl] = a;
			}
			pp++;}
	}

	bm->palette = (uint8_t*)malloc(768);
	memcpy(bm->palette, data + ds - 768, 768);

	return bm;
}

Bitmap *ConvertBitmapToR8G8B8A8(Bitmap *sb)
{
	Bitmap *bm = new Bitmap;
	bm->width = sb->width; bm->height = sb->height;
	bm->format = BMFORMAT_R8G8B8A8;
	int l = bm->width * bm->height;
	bm->pixels = (uint8_t*)malloc(l * 4);
	bm->palette = 0;
	if(sb->format == BMFORMAT_R8G8B8A8)
		memcpy(bm->pixels, sb->pixels, l * 4);
	else if(sb->format == BMFORMAT_P8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_RGBA(sb->palette[sb->pixels[i]*3+0],
							sb->palette[sb->pixels[i]*3+1],
							sb->palette[sb->pixels[i]*3+2],
							255);
	}
	else if(sb->format == BMFORMAT_R8G8B8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_RGBA(sb->pixels[i*3+0],
							sb->pixels[i*3+1],
							sb->pixels[i*3+2],
							255);
	}
	else if(sb->format == BMFORMAT_B8G8R8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_RGBA(sb->pixels[i*3+2],
							sb->pixels[i*3+1],
							sb->pixels[i*3+0],
							255);
	}
	else if(sb->format == BMFORMAT_B8G8R8A8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_RGBA(sb->pixels[i*4+2],
							sb->pixels[i*4+1],
							sb->pixels[i*4+0],
							sb->pixels[i*4+3]);
	}
	else	ferr("Failed to convert a bitmap to R8G8B8A8");

	return bm;
}

Bitmap *ConvertBitmapToB8G8R8A8(Bitmap *sb)
{
	Bitmap *bm = new Bitmap;
	bm->width = sb->width; bm->height = sb->height;
	bm->format = BMFORMAT_B8G8R8A8;
	int l = bm->width * bm->height;
	bm->pixels = (uint8_t*)malloc(l * 4);
	bm->palette = 0;
	if(sb->format == BMFORMAT_B8G8R8A8)
		memcpy(bm->pixels, sb->pixels, l * 4);
	else if(sb->format == BMFORMAT_P8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_BGRA(sb->palette[sb->pixels[i]*3+0],
							sb->palette[sb->pixels[i]*3+1],
							sb->palette[sb->pixels[i]*3+2],
							255);
	}
	else if(sb->format == BMFORMAT_R8G8B8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_BGRA(sb->pixels[i*3+0],
							sb->pixels[i*3+1],
							sb->pixels[i*3+2],
							255);
	}
	else if(sb->format == BMFORMAT_B8G8R8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_BGRA(sb->pixels[i*3+2],
							sb->pixels[i*3+1],
							sb->pixels[i*3+0],
							255);
	}
	else if(sb->format == BMFORMAT_R8G8B8A8)
	{
		for(int i = 0; i < l; i++)
			((uint32_t*)bm->pixels)[i] =	CM_BGRA(sb->pixels[i*4+0],
							sb->pixels[i*4+1],
							sb->pixels[i*4+2],
							sb->pixels[i*4+3]);
	}
	else	ferr("Failed to convert a bitmap to B8G8R8A8");

	return bm;
}

void FreeBitmap(Bitmap *bm)
{
	delete bm;
}

Bitmap::~Bitmap() {
	if (pixels) free(pixels);
	if (palette) free(palette);
}

void BitmapBlit32(Bitmap *db, int dx, int dy, Bitmap *sb, int sx, int sy, int sw, int sh)
{
	//printf("BitmapBlit32: dx = %i, dy = %i, sx = %i, sy = %i, sw = %i, sh = %i\n", dx, dy, sx, sy, sw, sh);
	for(int y = sy; y < (sy+sh); y++)
	for(int x = sx; x < (sx+sw); x++)
		((uint32_t*)db->pixels)[((y-sy+dy)*db->width+(x-sx+dx))] = ((uint32_t*)sb->pixels)[(y*sb->width+x)];
}

void write8(FILE *f, uint8_t a) {fwrite(&a, 1, 1, f);}
void write16(FILE *f, uint16_t a) {fwrite(&a, 2, 1, f);}
void write32(FILE *f, uint32_t a) {fwrite(&a, 4, 1, f);}

void WritePCXHeader(FILE *f, int w, int h, int np)
{
	int i;
	write8(f, 10); write8(f, 5); write8(f, 1); write8(f, 8);
	write32(f, 0);
	write16(f, w-1); write16(f, h-1);
	write16(f, w); write16(f, h);
	for(i = 0; i < 12; i++) write32(f, 0);
	write8(f, 0);
	write8(f, np);
	write16(f, w + (w&1));
	write16(f, 1);
	write16(f, 0);
	for(i = 0; i < 14; i++) write32(f, 0);
}

void WritePCXData(FILE *f, const uint8_t *pix, int w, int h, int np)
{
	int y, p, x;
	for(y = 0; y < h; y++)
	for(p = 0; p < np; p++)
	{
		const uint8_t *sl = pix + y*w*np + p;
		const uint8_t *d = sl;
		uint8_t o; // = *d;
		int cnt = 1;
		//d += np;
		for(x = 0; x < w-1; x++)
		{
			o = *d;
			d += np;
			if((*d != o) || (cnt >= 63))
			{
				if((cnt > 1) || (o >= 0xC0))
					write8(f, 0xC0 | cnt);
				write8(f, o);
				cnt = 1;
			}
			else cnt++;
		}
		if((cnt > 1) || (*d >= 0xC0))
			write8(f, 0xC0 | cnt);
		write8(f, *d);
		if(w & 1) write8(f, 0);
	}
}

void SaveBitmapPCX(Bitmap *bm, const char *fname)
{
	FILE *f = fopen(fname, "wb"); if(!f) ferr("Cannot open \"%s\" for writing PCX file.", fname);
	switch(bm->format)
	{
		case BMFORMAT_P8:
			WritePCXHeader(f, bm->width, bm->height, 1);
			WritePCXData(f, bm->pixels, bm->width, bm->height, 1);
			write8(f, 12);
			fwrite(bm->palette, 768, 1, f);
			break;
		case BMFORMAT_R8G8B8:
			WritePCXHeader(f, bm->width, bm->height, 3);
			WritePCXData(f, bm->pixels, bm->width, bm->height, 3);
			break;
		default:
			ferr("SaveBitmapPCX doesn't support format %i.", bm->format);
	}
	fclose(f);
}

Bitmap* ResizeBitmap(const Bitmap& original, int nwidth, int nheight) {
	Bitmap* _res = new Bitmap;
	Bitmap& res = *_res;
	res.width = nwidth;
	res.height = nheight;
	res.format = original.format;
	int npitch = nwidth * 4;
	int totalSize = npitch * nheight;
	res.pixels = (uint8_t*)malloc(totalSize);
	res.palette = nullptr;
	stbir_resize_uint8(original.pixels, original.width, original.height, original.width * 4, res.pixels, res.width, res.height, npitch, 4);
	return _res;
}