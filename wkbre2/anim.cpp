#include "anim.h"
#include "util/BinaryReader.h"
#include "file.h"

void Anim::load(const char *filename) {
	char *fcnt; int fsize;
	LoadFile(filename, &fcnt, &fsize);
	BinaryReader br(fcnt);
	std::string sign = br.readString(4);
	if (sign != "Anim")
		return;
	uint32_t ver = br.readUint32();
	this->meshname = br.readStringZ();
	this->duration = br.readUint32();
	br.seek(4 * 4); // sphere
	this->numVertices = br.readUint32();
	uint32_t numVertexGroups = this->numVertices / 3 + ((this->numVertices % 3) ? 1 : 0);
	for (Anim::AnimPosCoord &pc : this->coords) {
		pc.numFrames = br.readUint32();
		pc.frameTimes = new uint32_t[pc.numFrames];
		pc.verts = new uint32_t*[pc.numFrames];
		pc.vertadd = new float[pc.numFrames];
		pc.vertmul = new float[pc.numFrames];
		for (uint32_t f = 0; f < pc.numFrames; f++) {
			pc.frameTimes[f] = br.readUint32();
		}
		for (uint32_t f = 0; f < pc.numFrames; f++) {
			pc.vertadd[f] = br.readFloat();
			pc.vertmul[f] = br.readFloat();
			pc.verts[f] = new uint32_t[numVertexGroups];
			for (uint32_t g = 0; g < numVertexGroups; g++)
				pc.verts[f][g] = br.readUint32();
		}
	}
}