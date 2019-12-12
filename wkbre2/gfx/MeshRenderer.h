#pragma once

struct Mesh;

struct MeshRenderer {
	virtual void render(Mesh *mesh) = 0;
};
