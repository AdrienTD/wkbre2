#include "anim.h"
#include "util/BinaryReader.h"
#include "file.h"
#include "mesh.h"

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

const float* Anim::interpolate(uint32_t animTime, const Mesh& mesh)
{
	static std::vector<float> animverts;
	animverts.resize(3 * this->numVertices);
	if ((int32_t)animTime < 0) animTime = 0;
	int animtime = animTime % this->duration;
	for (int c = 0; c < 3; c++) {
		auto& ac = this->coords[c];
		int cf = 0;
		for (; cf < ac.numFrames - 1; cf++)
			if (ac.frameTimes[cf + 1] >= animtime)
				break;
		int frame = cf;
		for (int i = 0; i < this->numVertices; i++) {
			float z1 = ((ac.verts[frame][i / 3] >> (11 * (i % 3))) & 1023) / 1023.0f;
			float z2 = ((ac.verts[frame + 1][i / 3] >> (11 * (i % 3))) & 1023) / 1023.0f;
			float v1 = ac.vertadd[frame] + ac.vertmul[frame] * z1;
			float v2 = ac.vertadd[frame + 1] + ac.vertmul[frame + 1] * z2;
			int rmi = mesh.vertexRemapper[i];
			animverts[3 * rmi + c] = v1 + (v2 - v1) * (animtime - ac.frameTimes[frame]) / (ac.frameTimes[frame + 1] - ac.frameTimes[frame]);
		}
	}
	return animverts.data();
}
