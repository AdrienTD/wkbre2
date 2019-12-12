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

#define BMFORMAT_P8 1
#define BMFORMAT_R8G8B8 2
#define BMFORMAT_B8G8R8 3
#define BMFORMAT_R8G8B8A8 4
#define BMFORMAT_B8G8R8A8 5
#define CM_BGRA(r, g, b, a) ( (b) | ((g)<<8) | ((r)<<16) | ((a)<<24) )
#define CM_RGBA(r, g, b, a) ( (r) | ((g)<<8) | ((b)<<16) | ((a)<<24) )

struct Bitmap
{
	unsigned int width, height, format; unsigned char *pixels, *palette;

	//void loadFromTGA(void *data, size_t size);
	//void loadFromPCX(void *data, size_t size);

	Bitmap() : width(0), height(0), format(0), pixels(0), palette(0) {}
	~Bitmap();
};

Bitmap *LoadBitmap(const char *fn);
Bitmap *LoadTGA(const char *data, int ds);
Bitmap *LoadPCX(const char *data, int ds);
Bitmap *ConvertBitmapToR8G8B8A8(Bitmap *sb);
Bitmap *ConvertBitmapToB8G8R8A8(Bitmap *sb);
void FreeBitmap(Bitmap *bm);
void BitmapBlit32(Bitmap *db, int dx, int dy, Bitmap *sb, int sx, int sy, int sw, int sh);
void SaveBitmapPCX(Bitmap *bm, const char *fname);
