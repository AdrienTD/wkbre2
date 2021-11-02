// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct Mesh;

struct MeshRenderer {
	virtual void render(Mesh *mesh) = 0;
};
