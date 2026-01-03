#include "StampdownPlan.h"
#include "common.h"
#include "gameset/gameset.h"
#include "gameset/GameObjBlueprint.h"
#include "tags.h"
#include "terrain.h"

StampdownPlan StampdownPlan::getStampdownPlan(
	CommonGameState* gameState, CommonGameObject* playerObj, const GameObjBlueprint* blueprint, Vector3 position, Vector3 orientation)
{
	StampdownPlan plan;

	static const int wallBuildingTypes[] = {
		Tags::BUILDINGTYPE_WALL,
		Tags::BUILDINGTYPE_WALL_CORNER_IN,
		Tags::BUILDINGTYPE_WALL_CORNER_OUT,
		Tags::BUILDINGTYPE_WALL_CROSSROADS
	};

	if (blueprint->buildingType == Tags::BUILDINGTYPE_CIVIC || blueprint->buildingType == Tags::BUILDINGTYPE_CIVIC_CENTRE) {
		using CityRectangle = CommonGameObject::CityRectangle;
		
		const auto cityContainsTile = [](const std::vector<CityRectangle>& cityRectangles, int x, int z) -> bool {
			for (const auto& rect : cityRectangles) {
				if (rect.xStart <= x && x <= rect.xEnd && rect.yStart <= z && z <= rect.yEnd) {
					return true;
				}
			}
			return false;
			};
		const auto cityIntersectsWithRect = [](const std::vector<CityRectangle>& cityRectangles, const CityRectangle& rect) -> bool {
			for (const auto& cityRect : cityRectangles) {
				bool intersects_x = (cityRect.xStart <= rect.xStart && rect.xStart <= cityRect.xEnd)
					|| (cityRect.xStart <= rect.xEnd && rect.xEnd <= cityRect.xEnd);
				bool intersects_y = (cityRect.yStart <= rect.yStart && rect.yStart <= cityRect.yEnd)
					|| (cityRect.yStart <= rect.yEnd && rect.yEnd <= cityRect.yEnd);
				if (intersects_x && intersects_y) {
					return true;
				}
			}
			return false;
			};

		const GameSet* gameSet = gameState->gameSet.get();
		const GameObjBlueprint* bpWall = gameSet->findBlueprint(Tags::GAMEOBJCLASS_BUILDING, "Stockade Foundation");
		const GameObjBlueprint* bpWallCornerIn = gameSet->findBlueprint(Tags::GAMEOBJCLASS_BUILDING, "Stockade Corner In Foundation");
		const GameObjBlueprint* bpWallCornerOut = gameSet->findBlueprint(Tags::GAMEOBJCLASS_BUILDING, "Stockade Corner Out Foundation");
		const GameObjBlueprint* bpWallCrossPiece = gameSet->findBlueprint(Tags::GAMEOBJCLASS_BUILDING, "Stockade Cross Piece Foundation");
		Footprint* footprint = blueprint->footprint;
		if (bpWall && footprint) {
			auto [tileWidth, tileHeight] = gameState->terrain->getNumPlayableTiles();

			// remove offset applied by building from position
			position -= Vector3(footprint->originX, 0.0f, footprint->originZ);

			plan.cityRectangle.xStart = std::clamp(footprint->wallOriginDefaultMinX + static_cast<int>(position.x / 5.0f), 0, tileWidth - 1);
			plan.cityRectangle.yStart = std::clamp(footprint->wallOriginDefaultMinY + static_cast<int>(position.z / 5.0f), 0, tileHeight - 1);
			plan.cityRectangle.xEnd = std::clamp(footprint->wallOriginDefaultMaxX + static_cast<int>(position.x / 5.0f), 0, tileWidth - 1);
			plan.cityRectangle.yEnd = std::clamp(footprint->wallOriginDefaultMaxY + static_cast<int>(position.z / 5.0f), 0, tileHeight - 1);

			CommonGameObject* city = nullptr;
			std::vector<CityRectangle> cityRectangles;
			for (const auto& [bpid, objlist] : playerObj->children) {
				for (auto* candidate : objlist) {
					if (candidate->blueprint->bpClass == Tags::GAMEOBJCLASS_CITY) {
						if (cityIntersectsWithRect(candidate->cityRectangles, plan.cityRectangle)) {
							city = candidate;
							cityRectangles = city->cityRectangles;
							plan.ownerObject = city->id;
							break;
						}
					}
				}
			}

			if (!city) {
				plan.newOwnerBlueprint = gameSet->findBlueprint(Tags::GAMEOBJCLASS_CITY, "New City");
			}

			cityRectangles.push_back(plan.cityRectangle);

			for (int wall_x = footprint->wallOriginDefaultMinX; wall_x <= footprint->wallOriginDefaultMaxX; ++wall_x) {
				for (int wall_y = footprint->wallOriginDefaultMinY; wall_y <= footprint->wallOriginDefaultMaxY; ++wall_y) {
					Vector3 wallPos = position + Vector3(wall_x, 0, wall_y) * 5.0f;
					int wx = static_cast<int>(position.x / 5.0f) + wall_x;
					int wz = static_cast<int>(position.z / 5.0f) + wall_y;

					if (!(0 <= wx && wx < tileWidth && 0 <= wz && wz < tileHeight))
						continue;

					// are we on the border of the new rectangle?
					if (wall_x == footprint->wallOriginDefaultMinX || wall_x == footprint->wallOriginDefaultMaxX
						|| wall_y == footprint->wallOriginDefaultMinY || wall_y == footprint->wallOriginDefaultMaxY) {
						enum TileType
						{
							TOutside,
							TInside,
							TWall
						};
						auto tileType = [&](int tx, int tz) {
							if (cityContainsTile(cityRectangles, tx, tz)) {
								if (!gameState->tiles)
									return TInside;
								//auto* tileBuilding = tiles[tz * tileWidth + tx].building.getFrom<Server>();
								//if (tileBuilding) {
								for (const auto& tileObjRef : gameState->tiles[tz * tileWidth + tx].objList) {
									if (CommonGameObject* tileBuilding = tileObjRef.getFrom(gameState)) {
										auto it = std::find(std::begin(wallBuildingTypes), std::end(wallBuildingTypes), tileBuilding->blueprint->buildingType);
										if (it != std::end(wallBuildingTypes)) {
											return TWall;
										}
									}
								}
								return TInside;
							}
							return TOutside;
							};

						const GameObjBlueprint* needWall = nullptr;
						float needAngle = 0.0f;

						TileType leftType = tileType(wx - 1, wz);
						TileType rightType = tileType(wx + 1, wz);
						TileType bottomType = tileType(wx, wz - 1);
						TileType topType = tileType(wx, wz + 1);
						//if (leftType != TOutside && rightType != TOutside && bottomType != TOutside && topType != TOutside) {
						//	needWall = bpWallCrossPiece;
						//	needAngle = 0.0f;
						//}
						if (leftType == TOutside && rightType != TOutside && topType != TOutside && bottomType == TOutside) {
							// |.
							needWall = bpWallCornerOut;
							needAngle = 0.5f;
						}
						else if (leftType != TOutside && rightType == TOutside && topType != TOutside && bottomType == TOutside) {
							// .|
							needWall = bpWallCornerOut;
							needAngle = 0.75f;
						}
						else if (leftType != TOutside && rightType == TOutside && topType == TOutside && bottomType != TOutside) {
							// '|
							needWall = bpWallCornerOut;
							needAngle = 0.0f;
						}
						else if (leftType == TOutside && rightType != TOutside && topType == TOutside && bottomType != TOutside) {
							// |'
							needWall = bpWallCornerOut;
							needAngle = 0.25f;
						}
						else if (leftType != TOutside && rightType == TOutside) {
							needWall = bpWall;
							needAngle = 0.25f;
						}
						else if (bottomType != TOutside && topType == TOutside) {
							needWall = bpWall;
							needAngle = 0.5f;
						}
						else if (leftType == TOutside && rightType != TOutside) {
							needWall = bpWall;
							needAngle = 0.75f;
						}
						else if (bottomType == TOutside && topType != TOutside) {
							needWall = bpWall;
							needAngle = 0.0f;
						}

						if (needWall) {
							StampdownCreateAction& creation = plan.toCreate.emplace_back();
							creation.blueprint = needWall;
							creation.position = wallPos;
							creation.orientation = Vector3(0.0f, needAngle * 3.1415f * 2.0f, 0.0f);
						}
					}
					else {
						if (!gameState->tiles)
							continue;
						for (const auto& tileObjRef : gameState->tiles[wz * tileWidth + wx].objList) {
							if (CommonGameObject* tileBuilding = tileObjRef.getFrom(gameState)) {
								auto it = std::find(std::begin(wallBuildingTypes), std::end(wallBuildingTypes), tileBuilding->blueprint->buildingType);
								if (it != std::end(wallBuildingTypes)) {
									// wall now inside city, remove it
									StampdownRemoveAction& removal = plan.toRemove.emplace_back();
									removal.object = tileBuilding->id;
								}
							}
						}
					}
				}
			}
		}
	}

	plan.status = Status::Valid; // temporary
	return plan;
}
