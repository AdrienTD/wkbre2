#include "Order.h"
#include "tags.h"
#include "gameset/OrderBlueprint.h"

void OrderConfiguration::addOrder(OrderBlueprint * orderBlueprint, int assignMode)
{
	Order neworder(this->nextOrderId++, orderBlueprint);
	for (TaskBlueprint *taskBp : orderBlueprint->tasks)
		neworder.tasks.push_back(new Task(neworder.nextTaskId++, taskBp));
	switch (assignMode) {
	case Tags::ORDERASSIGNMODE_DO_FIRST:
		this->orders.push_front(std::move(neworder));
		break;
	default:
	case Tags::ORDERASSIGNMODE_DO_LAST:
		this->orders.push_back(std::move(neworder));
		break;
	case Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE:
		break;
	}
}
