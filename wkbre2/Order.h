#pragma once

#include <vector>
#include <queue>
#include <deque>

struct OrderBlueprint;
struct TaskBlueprint;

struct Order;
struct Task;
struct Trigger;

enum OrderTaskState {
	OTS_UNINITIALISED = 0,
	OTS_PROCESSING,
	OTS_SUSPENDED,
	OTS_ABORTED,
	OTS_CANCELLED,
	OTS_TERMINATED
};

struct Order {
	int id, state = OTS_UNINITIALISED;
	OrderBlueprint *blueprint;
	std::vector<Task*> tasks;
	int nextTaskId = 0;
	Task *currentTask;

	Order(int id, OrderBlueprint *blueprint) : id(id), blueprint(blueprint) {}

	void init();
	void start();
	void suspend();
	void resume();
};

struct Task {
	int id, state = OTS_UNINITIALISED;
	TaskBlueprint *blueprint;
	//Order *order;
	bool firstExecution = true, triggersStarted = false;
	std::vector<Trigger*> triggers;
	Task(int id, TaskBlueprint *blueprint) : id(id), blueprint(blueprint) {}
};

struct Trigger {
	virtual void update() = 0;
};

struct OrderConfiguration {
	std::deque<Order> orders;
	int nextOrderId = 0;

	void addOrder(OrderBlueprint *orderBlueprint, int assignMode);
};
