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

#include "growbuffer.h"

char *GrowBuffer::addSize(int ds)
{
	char *o;
	while((apnt + ds) > memsize)
	{
		char *nm;
		nm = (char*)realloc(memory, memsize + GROWBUF_ALLOC_STEP);
		if(!nm) ferr("Failed to reallocate GrowBuffer for adding data.");
		memset(nm + memsize, 0, GROWBUF_ALLOC_STEP);
		memory = nm; memsize += GROWBUF_ALLOC_STEP;
	}
	o = memory + apnt;
	apnt += ds;
	if(apnt & (align-1))
		apnt = (apnt & (~(align-1))) + align;
	return o;
}

/*void GrowBuffer::addData(char *data, int ds)
{
	memcpy(addSize(ds), data, ds);
}*/

void GrowBuffer::clear()
{
	if(!apnt) return;
	free(memory);
	memory = (char*)malloc(memsize = GROWBUF_ALLOC_STEP);
	if(!memory) ferr("Failed to clear GrowBuffer.");
	apnt = 0;
}
