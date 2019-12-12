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

#pragma once

#include "util.h"
#include <cstdlib>
#include <cstring>

#define GROWBUF_ALLOC_STEP 512

class GrowBuffer
{
public:
	char *memory; int memsize, apnt, align;

	GrowBuffer(int a = 1)
	{
		memory = (char*)malloc(memsize = GROWBUF_ALLOC_STEP);
		if(!memory) ferr("Failed to init. GrowBuffer.");
		apnt = 0; align = a;
	}
	~GrowBuffer() {free(memory);}

	char *addSize(int ds);
	void addData(const char *data, int ds) {memcpy(addSize(ds), data, ds);}
	void clear();
};

template <class T> class GrowList
{
public:
	GrowBuffer gb; unsigned int len;
	GrowList() {len = 0;}
	T *addp() {len++; return (T*)gb.addSize(sizeof(T));}
	unsigned int add(T a) {gb.addData((char*)&a, sizeof(T)); return len++;}
	void clear() {gb.clear(); len = 0;}
	T get(unsigned int n)
	{
#ifndef WKBRE_RELEASE
		if(n >= len) ferr("GrowList subscript overflow.");
#endif
		return *(((T*)gb.memory)+n);
	}
	T *getpnt(unsigned int n)
	{
#ifndef WKBRE_RELEASE
		if(n >= len) ferr("GrowList::getpnt subscript overflow.");
#endif
		return ((T*)gb.memory)+n;
	}
	T &getref(unsigned int n)
	{
#ifndef WKBRE_RELEASE
		if(n >= len) ferr("GrowList::getref subscript overflow.");
#endif
		return *(((T*)gb.memory)+n);
	}
	unsigned int find(T d)
	{
		for(unsigned int i = 0; i < len; i++)
			if(*(((T*)gb.memory)+i) == d)
				return i;
		return -1;
	}
	unsigned int has(T d)
	{
		for(unsigned int i = 0; i < len; i++)
			if(*(((T*)gb.memory)+i) == d)
				return 1;
		return 0;
	}
	T operator[](unsigned int n) {return this->get(n);}
};

class GrowStringList
{
public:
	GrowBuffer gbData; GrowList<unsigned int> gbPnt; unsigned int len;
	GrowStringList() {len = 0;}
	unsigned int add(const char *str)
	{
		gbPnt.add(gbData.apnt);
		gbData.addData(str, strlen(str)+1);
		return len++;
	}
	void clear() {gbData.clear(); gbPnt.clear(); len = 0;}
	char *get(unsigned int n)
	{
		char *s = gbData.memory + gbPnt.get(n);
		char *r = new char[strlen(s)];
		strcpy(r, s);
		return r;
	}
	char *getdp(unsigned int n) {return gbData.memory + gbPnt.get(n);}
	char *operator[](unsigned int n) {return this->getdp(n);}
	int has(const char *str)
	{
		for(unsigned int i = 0; i < len; i++)
			if(!_stricmp(gbData.memory + gbPnt.get(i), str))
				return 1;
		return 0;
	}
	int find(const char *str)
	{
		for(unsigned int i = 0; i < len; i++)
			if(!_stricmp(gbData.memory + gbPnt.get(i), str))
				return i;
		return -1;
	}
	int find_cs(const char *str)
	{
		for(unsigned int i = 0; i < len; i++)
			if(!strcmp(gbData.memory + gbPnt.get(i), str))
				return i;
		return -1;
	}
	int find_backwards(const char *str)
	{
		for(unsigned int i = len-1; i >= 0; i--)
			if(!_stricmp(gbData.memory + gbPnt.get(i), str))
				return i;
		return -1;
	}
};
