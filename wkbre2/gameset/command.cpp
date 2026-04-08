// wkbre2 - WK Engine Reimplementation
// (C) 2021AdrienTD
// Licensed under the GNU General Public License 3

#include "command.h"
#include "gameset.h"
#include "util/util.h"
#include "../server.h"
#include "../window.h"
#include "../gameset/ScriptContext.h"
#include <algorithm>

void Command::parse(GSFileParser &gsf, GameSet &gs) {
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();

		if (strtag == "START_SEQUENCE")
			startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (strtag == "USE_ORDER")
			order = gs.orders.readPtr(gsf);
		else if (strtag == "CURSOR") {
			auto path = gsf.nextString(true);
			if (path == "CURSOR") // needs to bypass one oddity
				path = gsf.nextString(true);
			cursor = WndCreateCursor(path.c_str());
		}
		else if (strtag == "CURSOR_CONDITION")
			cursorConditions.push_back(gs.equations.readIndex(gsf));
		else if (strtag == "ICON_CONDITION")
			iconConditions.push_back(gs.equations.readIndex(gsf));
		else if (strtag == "CURSOR_AVAILABLE") {
			GSCondition* cond = gs.conditions.readPtr(gsf);
			WndCursor* cursor = WndCreateCursor(gsf.nextString(true).c_str());
			cursorAvailable.push_back({ cond, cursor });
		}
		else if (strtag == "BUTTON_ENABLED")
			buttonEnabled = gsf.nextString(true);
		else if (strtag == "BUTTON_WAIT")
			buttonWait = gsf.nextString(true);
		else if (strtag == "BUTTON_DEPRESSED")
			buttonDepressed = gsf.nextString(true);
		else if (strtag == "BUTTON_HIGHLIGHTED")
			buttonHighlighted = gsf.nextString(true);
		else if (strtag == "BUTTON_AVAILABLE")
			buttonAvailable = gsf.nextString(true);
		else if (strtag == "BUTTON_IMPOSSIBLE")
			buttonImpossible = gsf.nextString(true);
		else if (strtag == "CONDITION_IMPOSSIBLE")
			conditionsImpossible.push_back(gs.conditions.readPtr(gsf));
		else if (strtag == "CONDITION_WAIT")
			conditionsWait.push_back(gs.conditions.readPtr(gsf));
		else if (strtag == "CONDITION_WARNING")
			conditionsWarning.push_back(gs.conditions.readPtr(gsf));
		else if (strtag == "STAMPDOWN_OBJECT")
			stampdownObject = gs.readObjBlueprintPtr(gsf);
		else if (strtag == "DEFAULT_HINT") {
			defaultHint = gsf.nextString(false);
			while (!gsf.eol) {
				defaultHintValues.push_back(ReadValueDeterminer(gsf, gs));
			}
		}
		else if (strtag == "FLAG") {
			auto flagName = gsf.nextString();
			if (flagName == "CAN_BE_ASSIGNED_TO_SPAWNED_UNITS") {
				canBeAssignedToSpawnedUnits = true;
			}
		}
		else if (strtag == "END_COMMAND")
			return;
		gsf.advanceLine();
	}
	ferr("Command reached end of file without END_COMMAND!");
}

bool Command::canBeExecuted(ServerGameObject* self, ServerGameObject* target) const
{
	SrvScriptContext ctx(Server::instance, self);
	auto _1 = ctx.target.change(target);
	auto _2 = ctx.selectedObject.change(self);
	auto testCondition = [&ctx](GSCondition* cond) -> bool {return cond->test->booleval(&ctx); };
	auto testEquation = [&ctx](int equ) -> bool { return equ != -1 ? Server::instance->gameSet->equations[equ]->booleval(&ctx) : true; };

	return std::all_of(this->conditionsImpossible.begin(), this->conditionsImpossible.end(), testCondition)
		&& std::all_of(this->conditionsWait.begin(), this->conditionsWait.end(), testCondition)
		&& (std::all_of(this->iconConditions.begin(), this->iconConditions.end(), testEquation)
			|| std::all_of(this->cursorConditions.begin(), this->cursorConditions.end(), testEquation));
}

void Command::execute(ServerGameObject *self, ServerGameObject *target, int assignmentMode, const Vector3 &destination) const
{
	if (!canBeExecuted(self, target))
		return;

	SrvScriptContext ctx(Server::instance, self);
	auto _ = ctx.change(ctx.target, target);
	this->startSequence.run(&ctx);
	// TODO: order (what to do first?)
	if (this->order) {
		Order* neworder = self->orderConfig.addOrder(this->order, assignmentMode, target, destination, false);
		// Give blueprint for spawn tasks
		if (this->order->classType == Tags::ORDTSKTYPE_SPAWN) {
			auto& gs = *ctx.server->gameSet;
			auto name = gs.commands.getString(this);
			if (name.substr(0, 6) == "Spawn ") {
				int x = gs.objBlueprints[Tags::GAMEOBJCLASS_CHARACTER].names.getIndex(name.substr(6));
				if (x != -1) {
					const GameObjBlueprint* bp = &gs.objBlueprints[Tags::GAMEOBJCLASS_CHARACTER][x];
					((SpawnTask*)neworder->tasks[0].get())->setSpawnBlueprint(bp);
				}
			}
		}
	}
}