#include "FormationController.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <utility>

#include "server.h"
#include "gameset/GameObjBlueprint.h"

static std::pair<float, float> formationMemberPosition(FormationType type, int numMembers, float memberDistance, int member)
{
	if (numMembers <= 1) {
		return { 0.0f, 0.0f };
	}
	if (type == FormationType::Line) {
		const int numRows = (int)std::sqrt(numMembers / 2.0f);
		const int numColumns = numMembers / numRows;
		const float rowOffset = (numColumns - 1) * memberDistance / 2.0f;
		return { (member % numColumns) * memberDistance - rowOffset, (member / numColumns) * memberDistance };
	}
	if (type == FormationType::Column) {
		const int numColumns = (int)std::sqrt(numMembers / 2.0f);
		const float rowOffset = (numColumns - 1) * memberDistance / 2.0f;
		return { (member % numColumns) * memberDistance - rowOffset, (member / numColumns) * memberDistance };
	}
	if (type == FormationType::Wedge) {
		// we solve n(n+1)/2 = member
		const int row = (-1 + (int)std::sqrt(float(1 + 8 * member))) / 2;
		const int rowFirstMember = row * (row + 1) / 2;
		const float rowOffset = std::min(row, numMembers - rowFirstMember - 1) * memberDistance / 2.0f;
		return { (member - rowFirstMember) * memberDistance - rowOffset, row * memberDistance };
	}
	if (type == FormationType::Orb) {
		if (numMembers <= 2) {
			return { 0.0f, 0.0f };
		}
		const float angleStep = 2.0f * M_PI / numMembers;
		const float formationRadius = (memberDistance / 2.0f) / std::sin(angleStep / 2.0f);
		return { std::cos(angleStep * member) * formationRadius, std::sin(angleStep * member) * formationRadius };
	}
	return { 0.0f, 0.0f };
}

void FormationController::update()
{
	FormationType type = FormationType::Line;
	if (formation->blueprint->name == "Line")
		type = FormationType::Line;
	else if (formation->blueprint->name == "Column")
		type = FormationType::Column;
	else if (formation->blueprint->name == "Wedge")
		type = FormationType::Wedge;
	else if (formation->blueprint->name == "Orb")
		type = FormationType::Orb;

	Matrix transformMatrix = formation->getWorldMatrix();

	int numMembers = 0;
	for (auto& [childType, childList] : formation->children) {
		if (childType.bpClass() == Tags::GAMEOBJCLASS_CHARACTER) {
			numMembers += childList.size();
		}
	}

	int memberIndex = 0;
	for (auto& [childType, childList] : formation->children) {
		if (childType.bpClass() == Tags::GAMEOBJCLASS_CHARACTER) {
			for (CommonGameObject* character : childList) {
				ServerGameObject* srvCharacter = character->dyncast<ServerGameObject>();
				if (srvCharacter->orderConfig.getCurrentOrder() == nullptr) {
					const float memberRadius = 1.0f;
					const float memberOutDistance = 1.0f;
					const float memberDistance = memberOutDistance + 2.0f * memberRadius;
					auto [posX, posZ] = formationMemberPosition(type, numMembers, memberDistance, memberIndex);
					srvCharacter->setPosition(Vector3(posX, 0, posZ).transform(transformMatrix));
				}
				memberIndex += 1;
			}
		}
	}
}
