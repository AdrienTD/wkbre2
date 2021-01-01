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

#include <string>
#include <vector>

struct BCPFileData
{
	uint32_t offset, endos, size, unk, form;
};

struct BCPFile
{
	uint32_t id;
	uint64_t time;
	std::string name;
};

class BCPWriter
{
private:
	FILE * file;
	std::vector<BCPFileData> ftable;

	void write8(uint8_t n) { fwrite(&n, 1, 1, file); }
	void write16(uint16_t n) { fwrite(&n, 2, 1, file); }
	void write32(uint32_t n) { fwrite(&n, 4, 1, file); }

public:
	struct WDirectory
	{
		std::vector<WDirectory*> dirs;
		std::vector<BCPFile> files;
		std::string name;
		BCPWriter *writer;

		WDirectory* addDir(char *name);
		void insertFile(uint32_t id, char *name);
		~WDirectory();

		void write();
	};
	WDirectory root;

	BCPWriter(char *fn);
	uint32_t createFile(void *pnt, uint32_t siz);
	void finalize();
	void copyFile(char *fn);
};

extern char gamedir[384];
extern bool allowBCPPatches, allowDataDirectory, macFileNamesFallbackEnabled;

void LoadBCP(const char *fn);
void LoadFile(const char *fn, char **out, int *outsize, int extraBytes = 0);
//void TestBCP();
int FileExists(const char *fn);
std::vector<std::string> *ListFiles(const char *dn, std::vector<std::string> *gsl = nullptr);
std::vector<std::string> *ListDirectories(const char *dn);
//void SetGameDir();