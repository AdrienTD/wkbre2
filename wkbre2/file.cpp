// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "wkbre2.h"
#include "file.h"
#include "util/util.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include "util/StriCompare.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#define _access access
#endif

extern "C" {

//#include "lzrw_headers.h" // This redefines some types from the Windows API :/
// Instead we will define a part of the LZRW header directly here:
#define COMPRESS_ACTION_IDENTITY   0
#define COMPRESS_ACTION_COMPRESS   1
#define COMPRESS_ACTION_DECOMPRESS 2

#define COMPRESS_OVERRUN 1024
#define COMPRESS_MAX_COM 0x70000000
#define COMPRESS_MAX_ORG (COMPRESS_MAX_COM-COMPRESS_OVERRUN)

#define COMPRESS_MAX_STRLEN 255

void compress(unsigned short action, unsigned char *wrk_mem, unsigned char *src_adr, unsigned int src_len, unsigned char *dst_adr, unsigned int *p_dst_len);

}

#include <bzlib.h>

// Fixs for wkbre2 until refactor of file.cpp
uint32_t readInt(FILE *f) { uint32_t temp; fread(&temp, 4, 1, f); return temp; } 

#define MEM_REQ ( 4096*sizeof(char*) + 16 + 16 ) // TODO: Check if correct, there are rare crashes of mem corruption!!!

struct BCPDirectory
{
	BCPDirectory *dirs; int ndirs;
	BCPFile *files; int nfiles;
	std::string name;
	BCPDirectory *parent;
};

struct BCPReader
{
	FILE *bcpfile;
	int fentof;
	int nfiles, ftsize;
	BCPFileData *fent;
	int bcpver;
	BCPDirectory bcproot;

	std::string BCPReadString();
	void LookAtDir(BCPDirectory *bd, int isroot, BCPDirectory *prev);
	BCPReader(const char *fn);
	void ExtractFile(int id, char **out, int *outsize, int extraBytes);
	BCPFile *getFile(const char *fn);
	BCPDirectory *getDirectory(const char *dn);
	bool loadFile(const char *fn, char **out, int *outsize, int extraBytes);
	bool fileExists(const char *fn);
	void listFileNames(const char *dn, std::vector<std::string>* gsl);
	void listDirectories(const char *dn, std::vector<std::string>* gsl);
};

std::string g_gamePath = ".";
bool g_allowBCPPatches = false, g_allowDataDirectory = true, g_macFileNamesFallbackEnabled = true;
std::vector<BCPReader*> bcpacks;

std::string BCPReader::BCPReadString()
{
	uint8_t cs = (uint8_t)fgetc(bcpfile);
	std::string str(cs, 0);
	if (bcpver == 0)
		fread((char*)str.data(), cs, 1, bcpfile);
	else
		for (int i = 0; i < cs; i++) {
			str[i] = fgetc(bcpfile);
			fgetc(bcpfile);
		}
	return str;
}

void BCPReader::LookAtDir(BCPDirectory *bd, int isroot, BCPDirectory *prev)
{
	if(!isroot) bd->name = BCPReadString();
	bd->parent = prev;
	bd->nfiles = readInt(bcpfile);
	if(bd->nfiles)
	{
		bd->files = new BCPFile[bd->nfiles];
		for(int i = 0; i < bd->nfiles; i++)
		{
			bd->files[i].id = readInt(bcpfile);
			fread(&bd->files[i].time, 8, 1, bcpfile);
			bd->files[i].name = BCPReadString();
		}
	}
	bd->ndirs = readInt(bcpfile);
	if(bd->ndirs)
	{
		bd->dirs = new BCPDirectory[bd->ndirs];
		for(int i = 0; i < bd->ndirs; i++)
			LookAtDir(&bd->dirs[i], 0, bd);
	}
}

BCPReader::BCPReader(const char *fn)
{
	nfiles = 0;

	std::string abuf = g_gamePath + '/' + fn;
	bcpfile = fopen(abuf.c_str(), "rb");
	if(!bcpfile) ferr("Cannot load BCP file.");
	fseek(bcpfile, 9, SEEK_SET);
	bcpver = fgetc(bcpfile) - '0';
	fseek(bcpfile, 0x30, SEEK_SET);
	fentof = readInt(bcpfile);
	ftsize = readInt(bcpfile);

	fseek(bcpfile, fentof, SEEK_SET);
	nfiles = readInt(bcpfile);
	fent = (BCPFileData*)malloc(nfiles*20);
	if(!fent) ferr("Failed to allocate memory for the file position/size table.");
	fread(fent, nfiles*20, 1, bcpfile);

	fseek(bcpfile, fentof+4+nfiles*20, SEEK_SET);
	LookAtDir(&bcproot, 1, &bcproot);
}

void BCPReader::ExtractFile(int id, char **out, int *outsize, int extraBytes)
{
	char *min, *mout, *ws;

	static std::mutex bcpmutex;
	const std::lock_guard<std::mutex> bml(bcpmutex);

	fseek(bcpfile, fent[id].offset, SEEK_SET);
	int s = fent[id].endos - fent[id].offset;
	mout = (char*)malloc(fent[id].size+extraBytes); if(!mout) ferr("Cannot alloc mem for loading a file.");
	*out = mout;

	switch(fent[id].form)
	{
		case 1: // Uncompressed
			fread(mout, s, 1, bcpfile);
			*outsize = s;
			break;
		case 2: // Zero-sized file
			*outsize = 0;
			break;
		case 3: // bzip2-compressed file
			{int be;
			BZFILE *bz = BZ2_bzReadOpen(&be, bcpfile, 0, 0, NULL, 0);
			if(be != BZ_OK) ferr("Failed to initialize bzip2 decompression.");
			int l = BZ2_bzRead(&be, bz, mout, fent[id].size);
			if((be != BZ_OK) && (be != BZ_STREAM_END)) ferr("Failed to decompress bzip2 file.");
			BZ2_bzReadClose(&be, bz);
			*outsize = l;
			break;}
		case 4: // LZRW3-compressed file
			{ws = (char*)malloc(MEM_REQ);
			min = (char*)malloc(s);
			fread(min, s, 1, bcpfile);
			//*outsize = fent[id].size;
			unsigned int us = fent[id].size;
			compress(COMPRESS_ACTION_DECOMPRESS, (unsigned char*)ws, (unsigned char*)min, s, (unsigned char*)mout, &us);
			*outsize = (int)us;
			free(ws); free(min);
			break;}
		default:
			ferr("Cannot load a file with an unknown format.");
	}
}

BCPFile *BCPReader::getFile(const char *fn)
{
	char *p, *c; BCPDirectory *ad = &bcproot;
	std::string ftb = fn;
	p = (char*)ftb.c_str();
	for(auto &e : ftb)
		if(e == '/')
			e = '\\';
	while(1)
	{
lflp:		c = strchr(p, '\\');
		if(!c)
		{
			// A file!
			for(int i = 0; i < ad->nfiles; i++)
			if(!StrCICompare(p, ad->files[i].name))
			{
				// File found!
				return &ad->files[i];
			}
			break; // File not found.

		} else {
			// A directory!
			*c = 0;
			if(!strcmp(p, "."))
				{p = c+1; goto lflp;}
			if(!strcmp(p, ".."))
			{
				// Go one directory backwards
				p = c+1;
				ad = ad->parent;
				goto lflp;
			}
			for(int i = 0; i < ad->ndirs; i++)
			if(!StrCICompare(p, ad->dirs[i].name))
			{
				// Dir found!
				p = c+1;
				ad = &ad->dirs[i];
				goto lflp;
			}
			break; // Dir not found.
		}
	}
	return 0;
}

BCPDirectory *BCPReader::getDirectory(const char *dn)
{
	char *p, *c; BCPDirectory *ad = &bcproot;
	std::string ftb = dn;
	p = (char*)ftb.c_str();
	for (auto& e : ftb)
		if (e == '/')
			e = '\\';
	while(1)
	{
lflp:		c = strchr(p, '\\');
		if(c)
		{
			// A directory!
			*c = 0;
			if(!strcmp(p, "."))
				{p = c+1; goto lflp;}
			if(!strcmp(p, ".."))
			{
				// Go one directory backwards
				p = c+1;
				ad = ad->parent;
				goto lflp;
			}
			for(int i = 0; i < ad->ndirs; i++)
			if(!StrCICompare(p, ad->dirs[i].name))
			{
				// Dir found!
				p = c+1;
				ad = &ad->dirs[i];
				goto lflp;
			}
			return 0; // Dir not found.
		} else {
			// A directory!
			if(!*p) return ad;
			if(!strcmp(p, "."))
				return ad;
			if(!strcmp(p, ".."))
			{
				// Go one directory backwards
				ad = ad->parent;
				return ad;
			}
			for(int i = 0; i < ad->ndirs; i++)
			if(!StrCICompare(p, ad->dirs[i].name))
			{
				// Dir found!
				ad = &ad->dirs[i];
				return ad;
			}
			return 0; // Dir not found.
		}
	}
}

bool BCPReader::loadFile(const char *fn, char **out, int *outsize, int extraBytes)
{
	BCPFile *bf = getFile(fn);
	if(!bf) return 0;
	ExtractFile(bf->id, out, outsize, extraBytes);
	return 1;
}

bool BCPReader::fileExists(const char *fn)
{
	BCPFile *bf = getFile(fn);
	return bf ? 1 : 0;
}

void BCPReader::listFileNames(const char *dn, std::vector<std::string> *gsl)
{
	BCPDirectory *ad = getDirectory(dn);
	if(!ad) return;
	for(int i = 0; i < ad->nfiles; i++)
		if (std::find_if(gsl->begin(), gsl->end(), [&](const std::string& s) { return !StrCICompare(s, ad->files[i].name); }) == gsl->end())
			gsl->push_back(ad->files[i].name);
}

void BCPReader::listDirectories(const char *dn, std::vector<std::string> *gsl)
{
	BCPDirectory *ad = getDirectory(dn);
	if(!ad) return;
	for(int i = 0; i < ad->ndirs; i++)
		if (std::find_if(gsl->begin(), gsl->end(), [&](const std::string& s) { return !StrCICompare(s, ad->dirs[i].name); }) == gsl->end())
			gsl->push_back(ad->dirs[i].name);
}

BCPReader *mainbcp;

void LoadBCP(const char *fn)
{
	//mainbcp = new BCPack(fn);

	if(!g_allowBCPPatches)
		{bcpacks.push_back(new BCPReader(fn)); return;}
#ifdef _WIN32
	HANDLE hf; WIN32_FIND_DATA fnd;
	std::string sn = g_gamePath + "\\*.bcp";
	hf = FindFirstFile(sn.c_str(), &fnd);
	if(hf == INVALID_HANDLE_VALUE) return;
	do {
		bcpacks.push_back(new BCPReader(fnd.cFileName));
	} while(FindNextFile(hf, &fnd));
#endif
}

BCPReader *GetBCPWithMostRecentFile(const char *fn)
{
	BCPReader *bestpack = 0; uint64_t besttime = 0;
	for(BCPReader *t : bcpacks)
	{
		BCPFile *f = t->getFile(fn);
		if(!f) continue;
		if(f->time >= besttime)
		{
			bestpack = t;
			besttime = f->time;
		}
	}
	return bestpack;
}

std::string GetMacName(const char *fn)
{
	if (const char *sh = strrchr(fn, '\\'))
	{
		sh += 1;
		if (strlen(sh) > 31)
		{
			const char *ext = strrchr(sh, '.');
			int mmplen = 31 - (ext ? strlen(ext) : 0) - 1;
			if (mmplen < 0) mmplen = 0;

			return std::string(fn, sh) + std::string(sh, mmplen) + '_' + ext;
		}
	}
	return {};
}

void LoadFile(const char *fn, char **out, int *outsize, int extraBytes)
{
	BCPReader *bp = GetBCPWithMostRecentFile(fn);
	if(bp) if(bp->loadFile(fn, out, outsize, extraBytes)) return;

	// File not found in the BCPs. Find it in the "data", "saved" and "redata" directories.
	std::string sn; FILE *sf = 0;
	if(g_allowDataDirectory)
	{
		sn = g_gamePath + "/data/" + fn;
		sf = fopen(sn.c_str(), "rb");
	}
	if(!sf)
	{
		sn = g_gamePath + "/saved/" + fn;
		sf = fopen(sn.c_str(), "rb");
	}
#ifdef _WIN32
	if (!sf)
		if(HRSRC h = FindResourceA(NULL, fn, "REDATA"))
			if (HGLOBAL g = LoadResource(NULL, h))
			{
				void *p = LockResource(g);
				uint32_t siz = SizeofResource(NULL, h);
				void *mout = malloc(siz+extraBytes);
				memcpy(mout, p, siz);
				*out = (char*)mout;
				*outsize = siz;
				return;
			}
#endif
	if (!sf)
	{
		sn = std::string("redata/") + fn;
		sf = fopen(sn.c_str(), "rb");
	}
	if (!sf && g_macFileNamesFallbackEnabled)
		if (auto nfn = GetMacName(fn); !nfn.empty())
		{
			LoadFile(nfn.c_str(), out, outsize, extraBytes);
			return;
		}
	if(!sf)
		ferr("The file \"%s\" could not be found in the game directories/archives (data.bcp, saved, redata, data, ...).\n\nIf it is the first time you use this program, be sure that you have configured the GAME_DIR parameter in settings.txt . See help.htm for more details.", fn);
		//ferr("File cannot be found in data.bcp, in the \"saved\" folder and the \"redata\" directory.");
	fseek(sf, 0, SEEK_END);
	int ss = ftell(sf);
	fseek(sf, 0, SEEK_SET);
	char *mout = (char*)malloc(ss+extraBytes);
	if(!mout) ferr("No mem. left for reading a file from saved dir.");
	fread(mout, ss, 1, sf);
	fclose(sf);
	*out = mout; *outsize = ss;
	return;
}

int FileExists(const char *fn)
{
	//if(mainbcp->fileExists(fn)) return 1;
	for(BCPReader *bcp : bcpacks)
		if(bcp->fileExists(fn))
			return 1;

	// File not found in the BCPs. Find it in the "saved" and "redata" folder.
	std::string sn = g_gamePath + "/saved/" + fn;
	if (_access(sn.c_str(), 0) != -1) return 1;

#ifdef _WIN32
	if (FindResourceA(NULL, fn, "REDATA"))
		return 1;
#endif

	sn = std::string("redata/") + fn;
	if (_access(sn.c_str(), 0) != -1) return 1;

	if (g_allowDataDirectory) {
		sn = g_gamePath + "/data/" + fn;
		if (_access(sn.c_str(), 0) != -1) return 1;
	}

	if (g_macFileNamesFallbackEnabled)
		if (auto nfn = GetMacName(fn); !nfn.empty()) {
			auto r = FileExists(nfn.c_str());
			return r;
		}
	return 0;
}

void FindFiles(const char *sn, std::vector<std::string>* gsl)
{
#ifdef _WIN32
	HANDLE hf; WIN32_FIND_DATA fnd;
	hf = FindFirstFile(sn, &fnd);
	if(hf == INVALID_HANDLE_VALUE) return;
	do {
		if(!(fnd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			if (std::find_if(gsl->begin(), gsl->end(), [&](const std::string& s) { return !StrCICompare(s, fnd.cFileName); }) == gsl->end())
				gsl->push_back(fnd.cFileName);
	} while(FindNextFile(hf, &fnd));
#endif
}

std::vector<std::string>* ListFiles(const char *dn, std::vector<std::string>* gsl)
{
	if(!gsl) gsl = new std::vector<std::string>;

	//mainbcp->listFileNames(dn, gsl);
	for (BCPReader* bcp : bcpacks)
		bcp->listFileNames(dn, gsl);

	std::string sn;

	if(g_allowDataDirectory)
	{
		sn = g_gamePath + "/data/" + dn + "/*.*";
		FindFiles(sn.c_str(), gsl);
	}

	sn = g_gamePath + "/saved/" + dn + "/*.*";
	FindFiles(sn.c_str(), gsl);

	return gsl;
}

std::vector<std::string>* ListDirectories(const char *dn)
{
	std::vector<std::string>* gsl = new std::vector<std::string>;
	for (BCPReader* bcp : bcpacks)
		bcp->listDirectories(dn, gsl);
	return gsl;
}

BCPWriter::WDirectory* BCPWriter::WDirectory::addDir(const char *name)
{
	WDirectory* nwd = new WDirectory;
	nwd->writer = writer;
	nwd->name = name;
	this->dirs.push_back(nwd);
	return nwd;
}

void BCPWriter::WDirectory::insertFile(uint32_t id, const char *name)
{
	files.emplace_back();
	BCPFile* wf = &files.back();
	wf->id = id;
	wf->name = name;
	wf->time = 0;
}

BCPWriter::WDirectory::~WDirectory()
{
	for (size_t i = 0; i < dirs.size(); i++)
		delete dirs[i];
}

BCPWriter::BCPWriter(const char *fn)
{
	file = fopen(fn, "wb");
	if (!file) ferr("Failed to create BCP for writing.");

	// Every mod is owned by BC
	fwrite("PAK File 2.01 (c) Black Cactus Games Limited \xAD\x91\xE4", 48, 1, file);

	// Placeholder, will be overwritten at the end
	write32(0); write32(0);

	root.writer = this;
}

uint32_t BCPWriter::createFile(void *pnt, uint32_t siz)
{
	uint id = (uint)ftable.size();
	ftable.emplace_back();
	BCPFileData* e = &ftable.back();
	e->offset = ftell(file);
	e->size = siz;
	e->unk = 0;

	unsigned char *lzws = (unsigned char*)malloc(MEM_REQ);
	uint slz = siz + COMPRESS_OVERRUN; //4 + siz + (siz / 16) * 2 + 4;
	void *cplz = malloc(slz);
	compress(COMPRESS_ACTION_COMPRESS, lzws, (unsigned char*)pnt, siz, (unsigned char*)cplz, &slz);
	free(lzws);

	if (slz < siz) {
		e->form = 4;
		fwrite(cplz, slz, 1, file);
	}
	else {
		e->form = 1;
		fwrite(pnt, siz, 1, file);
	}
	free(cplz);

	e->endos = ftell(file);
	return id;
}

void BCPWriter::finalize()
{
	uint ftoff = ftell(file);
	write32((uint)ftable.size());
	for (uint i = 0; i < (uint)ftable.size(); i++)
	{
		BCPFileData& e = ftable[i];
		write32(e.offset);
		write32(e.endos);
		write32(e.size);
		write32(e.unk);
		write32(e.form);
	}
	root.write();
	uint eof = ftell(file);
	fseek(file, 48, SEEK_SET);
	write32(ftoff);
	write32(eof - ftoff);
	fclose(file);
}

void BCPWriter::WDirectory::write()
{
	writer->write32((uint)files.size());
	for (uint i = 0; i < (uint)files.size(); i++)
	{
		BCPFile &f = files[i];
		writer->write32(f.id);
		writer->write32(f.time & 0xFFFFFFFF);
		writer->write32(f.time >> 32);
		uint sl = f.name.size();
		writer->write8(sl);
		for (uint j = 0; j < sl; j++)
			writer->write16((unsigned char)f.name[j]);
	}
	writer->write32((uint)dirs.size());
	for (uint i = 0; i < (uint)dirs.size(); i++)
	{
		WDirectory *d = dirs[i];
		//WDirectory *d = dirs.at(i);
		uint sl = d->name.size();
		writer->write8(sl);
		for (uint j = 0; j < sl; j++)
			writer->write16((unsigned char)d->name[j]);
		dirs[i]->write();
	}
}

void BCPWriter::copyFile(const char *fn)
{
	BCPWriter::WDirectory *d = &root;
	std::string fncopy = fn;
	char *p = const_cast<char*>(fncopy.data());
	while (char *s = strchr(p, '\\')) {
		*s = 0;
		for (uint i = 0; i < (uint)d->dirs.size(); i++)
			if (!StrCICompare(d->dirs[i]->name, p)) {
				d = d->dirs[i];
				goto dirfnd;
			}
		d = d->addDir(p);
	dirfnd:
		p = s + 1;
	}
	char *fdat; int fsiz;
	LoadFile(fn, &fdat, &fsiz);
	uint id = createFile(fdat, fsiz);
	d->insertFile(id, p);
}
