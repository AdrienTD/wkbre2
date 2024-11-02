#pragma once

struct ServerGameObject;

enum class FormationType {
	Line,
	Column,
	Wedge,
	Orb
};

class FormationController
{
public:
	FormationController(ServerGameObject* formation) : formation(formation) {}
	void update();
private:
	ServerGameObject* formation;
	//FormationType type = FormationType::Line;
};