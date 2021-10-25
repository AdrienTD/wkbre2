#include "TrnTextureDb.h"
#include "file.h"
#include "util/GSFileParser.h"
#include <cstring>
#include <string>
#include <map>
#include "tags.h"

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
