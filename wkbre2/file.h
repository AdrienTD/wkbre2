// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <filesystem>
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

		WDirectory* addDir(const char *name);
		void insertFile(uint32_t id, const char *name);
		~WDirectory();

		void write();
	};
	WDirectory root;

	BCPWriter(const char *fn);
	uint32_t createFile(void *pnt, uint32_t siz);
	void finalize();
	void copyFile(const char *fn);
};

extern std::filesystem::path g_gamePath;
extern bool g_allowBCPPatches, g_allowDataDirectory, g_macFileNamesFallbackEnabled;

void LoadBCP(const char *fn);
void LoadFile(const char *fn, char **out, int *outsize, int extraBytes = 0);
//void TestBCP();
int FileExists(const char *fn);
std::vector<std::string> *ListFiles(const char *dn, std::vector<std::string> *gsl = nullptr);
std::vector<std::string> *ListDirectories(const char *dn);
//void SetGameDir();