#pragma once

#include <memory>
#include <vector>
//#include "values.h"

struct GameSet;
struct GSFileParser;
struct ObjectFinder;
struct OrderBlueprint;
struct ValueDeterminer;

struct WorkOrder {
	struct Assignment {
		enum QuantityType {
			INDIVIDUALS = 1,
			FRACTION_DOWN = 2,
			REMAINING_WORKERS = 3
		};
		enum DistributionType {
			DIST_PER = 1,
			DIST_BETWEEN = 2,
			DIST_AUTO_IDENTIFY = 3
		};
		QuantityType quantityType;
		ValueDeterminer* quantityValue;
		const OrderBlueprint* order;
		DistributionType distType;
		ObjectFinder* distFinder;
	};

	std::vector<Assignment> assignments;

	void parse(GSFileParser& gsf, const GameSet& gs);
};
