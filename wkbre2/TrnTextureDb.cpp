#include "TrnTextureDb.h"
#include "file.h"
#include "util/GSFileParser.h"
#include <cstring>
#include <string>
#include <map>
#include "tags.h"
#include "util/BinaryReader.h"

void TerrainTextureDatabase::load(const char * filename)
{
	char *file; int filesize;
	LoadFile(filename, &file, &filesize, 1);
	file[filesize] = 0;
	GSFileParser parser(file);

	std::map<std::string, TerrainTextureGroup*> groupNameMap;

	// Pass 1
	while (!parser.eof)
	{
		std::string strtag1 = parser.nextTag();
		if (strtag1 == "TEXTURE_GROUP") {
			TerrainTextureGroup *grp = new TerrainTextureGroup;
			grp->name = parser.nextString();
			groups.emplace(grp->name, grp);
			groupNameMap[grp->name] = grp;

			parser.advanceLine();
			// Pass 1 TEXTURE_GROUP
			while (!parser.eof) {
				std::string strtag2 = parser.nextTag();
				if (strtag2 == "TEXTURE") {
					TerrainTexture *tex = new TerrainTexture;
					tex->grp = grp;
					tex->file = parser.nextString();
					for (int* v : { &tex->startx, &tex->starty, &tex->width, &tex->height, &tex->id })
						*v = parser.nextInt();
					grp->textures.emplace(tex->id, tex);

					parser.advanceLine();
					// Parse TEXTURE
					while (!parser.eof) {
						std::string strtag3 = parser.nextTag();
						int edgedir = Tags::MAPTEXDIR_tagDict.getTagID(strtag3.c_str());
						if (strtag3 == "MATCHING") {
							TerrainTexture *tex = new TerrainTexture;
							tex->grp = grp;
							tex->file = parser.nextString();
							for (int* v : { &tex->startx, &tex->starty, &tex->width, &tex->height, &tex->id })
								*v = parser.nextInt();
							grp->textures.emplace(tex->id, tex);
						}
						else if (edgedir != -1) {
							//TerrainTextureEdge edge;

						}
						else if (strtag3 == "END_TEXTURE")
							break;
						parser.advanceLine();
					}
				}
				else if (strtag2 == "END_GROUP")
					break;
				parser.advanceLine();
			}
		}
		parser.advanceLine();
	}
}

void TerrainTextureDatabase::translate(const char* ltttFilePath)
{
	if (!FileExists(ltttFilePath)) {
		printf("No LTTT found\n");
		return;
	}
	char* lttt; int siz;
	LoadFile(ltttFilePath, &lttt, &siz);
	BinaryReader br{ lttt };
	uint32_t header = br.readUint32();
	uint32_t version = br.readUint32();

	std::vector<std::string> destNames, srcNames;
	destNames.resize(br.readUint16());
	for (auto& str : destNames)
		str = "Packed\\" + br.readStringZ() + ".pcx";
	srcNames.resize(br.readUint16());
	for (auto& str : srcNames)
		str = br.readStringZ();

	struct TransEntry {
		uint8_t position;
		std::string textureFilePath;
		bool operator==(const TransEntry& e) const { return position == e.position && stricmp(textureFilePath.c_str(), e.textureFilePath.c_str()) == 0; }
		bool operator<(const TransEntry& e) const { return (position != e.position) ? (position < e.position) : stricmp(textureFilePath.c_str(), e.textureFilePath.c_str()) < 0; }
	};
	uint32_t numTranslations = br.readUint32();
	std::map<TransEntry, TransEntry> translations;
	for (uint32_t i = 0; i < numTranslations; i++) {
		uint16_t srcIndex = br.readUint16();
		uint8_t srcPos = br.readUint8();
		uint16_t destIndex = br.readUint16();
		uint8_t destPos = br.readUint8();

		translations[{srcPos, srcNames[srcIndex]}] = { destPos, destNames[destIndex] };
	}

	free(lttt);

	for (auto& grp : groups) {
		for (auto& e_tex : grp.second->textures) {
			auto& tex = *e_tex.second;
			uint8_t pos = ((tex.starty / 64) << 2) | (tex.startx / 64);
			auto it = translations.find({ pos, tex.file });
			if (it != translations.end()) {
				auto& tr = it->second;
				tex.startx = (tr.position & 3) * 64;
				tex.starty = ((tr.position >> 2) & 3) * 64;
				tex.file = tr.textureFilePath;
			}
		}
	}
}

std::string TerrainTextureDatabase::getLTTTPath(const std::string& terrainFilePath)
{
	size_t namepos = terrainFilePath.rfind('\\');
	if (namepos != std::string::npos) namepos += 1; else namepos = 0;
	size_t endpos = terrainFilePath.rfind('.');
	std::string name = terrainFilePath.substr(namepos, (endpos == std::string::npos) ? endpos : endpos - namepos);
	return "Maps\\Map_Textures\\" + name + ".lttt";
}
