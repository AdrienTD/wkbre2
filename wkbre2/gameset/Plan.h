// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <memory>
#include <string>
#include <vector>

struct GSFileParser;
struct GameSet;
struct SrvScriptContext;

struct PlanNodeState {
};

struct PlanNodeBlueprint {
	virtual ~PlanNodeBlueprint() = default;
	// Parse plan node from gameset
	virtual void parse(GSFileParser& gsf, const GameSet& gs) = 0;
	// Create a state for this plan flow node
	virtual PlanNodeState* createState() = 0;
	// Reset the state
	virtual void reset(PlanNodeState* state) = 0;
	// Execute the flow node with given state and server script context.
	// Returns True if completed (can go to next node).
	// Returns False if not completed (must be reexecuted next time).
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) = 0;

	// Create and read plan node from the gameset
	static PlanNodeBlueprint* createFrom(const std::string& tag, GSFileParser& gsf, const GameSet& gs);
};

// Sequence of plan nodes
struct PlanNodeSequence {
	struct State {
		size_t currentNode = 0;
		std::vector<PlanNodeState*> nodeStates;
	};
	std::vector<std::unique_ptr<PlanNodeBlueprint>> nodes;
	void parse(GSFileParser& gsf, const GameSet& gs, const char* endtag);
	State createState() const;
	void reset(State* state) const;
	bool execute(State* state, SrvScriptContext* ctx) const;
};