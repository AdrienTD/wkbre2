#include "anim.h"
#include "util/BinaryReader.h"
#include "file.h"
#include "mesh.h"

int g_diag_animHits = 0, g_diag_animMisses = 0;

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
	this->sphereCenter = br.readVector3();
	this->sphereRadius = br.readFloat();
	this->numVertices = br.readUint32();
	uint32_t numVertexGroups = this->numVertices / 3 + ((this->numVertices % 3) ? 1 : 0);
	for (Anim::AnimPosCoord &pc : this->coords) {
		pc.numFrames = br.readUint32();
		pc.frameTimes.resize(pc.numFrames);
		pc.verts.resize(pc.numFrames);
		pc.vertadd.resize(pc.numFrames);
		pc.vertmul.resize(pc.numFrames);
		for (uint32_t f = 0; f < pc.numFrames; f++) {
			pc.frameTimes[f] = br.readUint32();
		}
		for (uint32_t f = 0; f < pc.numFrames; f++) {
			pc.vertadd[f] = br.readFloat();
			pc.vertmul[f] = br.readFloat();
			pc.verts[f].resize(numVertexGroups);
			for (uint32_t g = 0; g < numVertexGroups; g++)
				pc.verts[f][g] = br.readUint32();
		}
	}
	uint32_t numAttachPoints = br.readUint32();
	attachPoints.resize(numAttachPoints);
	for (uint32_t i = 0; i < numAttachPoints; i++) {
		auto& aap = attachPoints[i];
		aap.numFrames = br.readUint32();
		aap.frameTimes.resize(aap.numFrames);
		aap.states.resize(aap.numFrames);
		for (uint32_t& ft : aap.frameTimes)
			ft = br.readUint32();
		for (auto& st : aap.states) {
			st.position = br.readVector3();
			for (float& fl : st.orientation)
				fl = br.readFloat();
			st.on = br.readUint8() != 0;
		}
	}
	normalFrames.numFrames = br.readUint32();
	normalFrames.frameTimes.resize(normalFrames.numFrames);
	normalFrames.norms.resize(normalFrames.numFrames);
	for (uint32_t& ft : normalFrames.frameTimes)
		ft = br.readUint32();
	normalFrames.numNormals = br.readUint32();
	for (auto& frame : normalFrames.norms) {
		frame.resize(normalFrames.numNormals);
		for (uint8_t& n : frame)
			n = br.readUint8();
	}
}

const Vector3* Anim::interpolateNormals(uint32_t animTime, const Mesh& mesh)
{
	static std::vector<Vector3> animnorms;
	static std::pair<Anim*, uint32_t> prevcall = { nullptr, 0 };
	auto curcall = std::make_pair(this, animTime);
	if (curcall == prevcall)
		return animnorms.data();
	prevcall = curcall;

	animnorms.resize(normalFrames.numNormals);
	if ((int32_t)animTime < 0) animTime = 0;
	int animtime = animTime % this->duration;
	auto& ac = normalFrames;
	int cf = 0;
	for (; cf < ac.numFrames - 1; cf++)
		if (ac.frameTimes[cf + 1] >= animtime)
			break;
	int frame = cf;
	for (uint32_t i = 0; i < ac.numNormals; i++) {
		const auto& v1 = Mesh::s_normalTable[ac.norms[frame][i]];
		const auto& v2 = Mesh::s_normalTable[ac.norms[frame+1][i]];
		Vector3 vi = v1 + (v2 - v1) * (animtime - ac.frameTimes[frame]) / (ac.frameTimes[frame + 1] - ac.frameTimes[frame]);
		animnorms[mesh.normalRemapper[i]] = vi.normal();
	}
	return animnorms.data();
}

const float* Anim::interpolate(uint32_t animTime, const Mesh& mesh)
{
	static std::vector<float> animverts;
	static std::pair<Anim*, uint32_t> prevcall = { nullptr, 0 };
	auto curcall = std::make_pair(this, animTime);
	if (curcall == prevcall) {
		g_diag_animHits++; return animverts.data();
	}
	prevcall = curcall;
	g_diag_animMisses++;

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

AttachmentPointState Anim::getAPState(size_t index, uint32_t animTime)
{
	if ((int32_t)animTime < 0) animTime = 0; else animTime %= this->duration;
	auto& ap = this->attachPoints[index];
	size_t frame = ap.getAPFrame(animTime);
	float delta = (float)(animTime - ap.frameTimes[frame]) / (ap.frameTimes[frame + 1] - ap.frameTimes[frame]);

	AttachmentPointState state;
	state.position = ap.states[frame].position + (ap.states[frame + 1].position - ap.states[frame].position) * delta;
	// TODO: Orientation
	state.orientation = { 1.0f, 0.0f, 0.0f, 0.0f };
	state.on = ap.states[frame].on;
	return state;
}

size_t Anim::AnimAttachPoint::getAPFrame(uint32_t animTime)
{
	int frame = 0;
	for (; frame < numFrames - 1; frame++)
		if (frameTimes[frame + 1] >= animTime)
			break;
	return frame;
}
