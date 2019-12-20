#include "mesh.h"
#include "file.h"
#include <cassert>
#include "util/BinaryReader.h"

void Mesh::load(const char * filename)
{
	char *fcnt; int fsize;
	LoadFile(filename, &fcnt, &fsize);
	BinaryReader br(fcnt);
	std::string sign = br.readString(4);
	if (sign != "Mesh")
		return;
	uint32_t version = br.readUint32();
	uint32_t flags = br.readUint32();

	// Attachment points
	numAttachPoints = br.readUint16();
	attachPoints.resize(numAttachPoints);
	for (uint16_t i = 0; i < numAttachPoints; i++) {
		AttachmentPoint &ap = attachPoints[i];
		ap.tag = br.readStringZ();
		ap.staticState.position.x = br.readFloat();
		ap.staticState.position.y = br.readFloat();
		ap.staticState.position.z = br.readFloat();
		ap.staticState.orientation[0] = br.readFloat();
		ap.staticState.orientation[1] = br.readFloat();
		ap.staticState.orientation[2] = br.readFloat();
		ap.staticState.orientation[3] = br.readFloat();
		ap.staticState.on = br.readUint8();
		ap.path = br.readStringZ();
	}

	// Vertex positions
	numVertices = br.readUint16();
	vertices.resize(3*numVertices);
	for (uint16_t i = 0; i < 3*numVertices; i++)
		vertices[i] = br.readFloat();

	// Sphere
	this->sphereCenter = br.readVector3();
	this->sphereRadius = br.readFloat();

	// Vertex remapper
	if (!(flags & 1)) {
		uint16_t nremap = br.readUint16();
		assert(nremap == numVertices);
		this->vertexRemapper.resize(nremap);
		for (int i = 0; i < nremap; i++)
			this->vertexRemapper[i] = br.readUint16();
	}
	else {
		this->vertexRemapper.resize(numVertices);
		for (int i = 0; i < numVertices; i++)
			this->vertexRemapper[i] = i;
	}

	// Normals
	uint16_t nnorm = br.readUint16();
	uint32_t numElem = br.readUint32();
	br.seek(numElem * 2);

	// Materials
	uint32_t nmat = br.readUint16();
	this->materials.resize(nmat);
	for (int i = 0; i < nmat; i++)
	{
		Material &mat = this->materials[i];
		mat.alphaTest = br.readUint8();
		mat.textureFilename = br.readStringZ();
	}

	// Texture coordinates
	uint32_t nuvlist = br.readUint32();
	this->uvLists.resize(nuvlist);
	for (auto &uvlist : this->uvLists)
	{
		uint16_t ntexc = br.readUint16();
		uvlist.resize(2*ntexc);
		for (int u = 0; u < 2*ntexc; u++)
			uvlist[u] = br.readFloat();

	}

	// Groups
	uint32_t ngrp = br.readUint32();
	groupIndices.resize(ngrp);
	for (DynArray<IndexTuple> &group : groupIndices) {
		uint16_t ntup = br.readUint16();
		group.resize(ntup);
		for (IndexTuple &t : group) {
			t.vertex = br.readUint16();
			t.normal = br.readUint16();
			t.uv = br.readUint16();
		}
	}

	// Polygon List
	uint32_t npolylists = br.readUint32();
	polyLists.resize(npolylists);
	for (PolygonList &polylist : polyLists) {
		polylist.numVerts = br.readUint16();
		polylist.numNorms = br.readUint16();
		polylist.numUvs = br.readUint16();
		polylist.distance = br.readFloat();
		polylist.groups.resize(ngrp);
		int matcount = 0;
		for (PolyListGroup &grp : polylist.groups) {
			uint16_t group_size = br.readUint16();
			uint16_t vertex_list_length = br.readUint16();
			uint16_t material_index = br.readUint16();
			//assert(vertex_list_length == group_size * 3);
			assert(material_index == matcount++);
			grp.tupleIndex.resize(group_size);
			for (std::array<uint16_t, 3> &t : grp.tupleIndex) {
				for (int i = 0; i < 3; i++)
					t[i] = br.readUint16();
			}
		}
	}
}
