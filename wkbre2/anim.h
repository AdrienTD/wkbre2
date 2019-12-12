#pragma once

#include <cstdint>
#include <string>
#include <array>

struct AttachmentPointState;

struct Anim //: public Model
{
	struct AnimPosCoord
	{
		uint32_t numFrames;
		uint32_t *frameTimes;
		uint32_t **verts;
		float *vertadd, *vertmul;
	};

	struct AnimAttachPoint
	{
		uint32_t numFrames;
		uint32_t *frameTimes;
		AttachmentPointState *states;
	};

	//Mesh *mesh;
	std::string meshname;
	uint32_t duration;
	uint32_t numVertices;
	std::array<AnimPosCoord,3> coords;
	uint32_t numAttachPoints;
	AnimAttachPoint *attachPoints;
	/*
	Anim(char *fn);
	void CreateVertsFromTime(batchVertex *out, int tm, int grp);
	void loadMin();
	void prepare();
	void draw(int iwtcolor = 0);
	void drawInBatch(RBatch *batch, int grp, int uvl = 0, int dif = 0, int tm = 0);
	void getAttachPointPos(Vector3 *vout, int apindex, int tm = 0);
	bool isAttachPointOn(int apindex, int tm = 0);
	int hasAttachPointTurnedOn(int apindex, int tma, int tmb);
	*/

	void load(const char *filename);

	Anim() {}
	Anim(const char *filename) { load(filename); }
};
