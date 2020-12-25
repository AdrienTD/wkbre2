#pragma once

#include <vector>
#include <queue>
#include <deque>
#include "gameset/actions.h"
#include "GameObjectRef.h"

struct OrderBlueprint;
struct TaskBlueprint;
struct TriggerBlueprint;

struct Order;
struct Task;
struct Trigger;

struct ServerGameObject;
struct GameSet;
struct GSFileParser;

enum OrderTaskState {
	OTS_UNINITIALISED = 0,
	OTS_PROCESSING,
	OTS_SUSPENDED,
	OTS_ABORTED,
	OTS_CANCELLED,
	OTS_TERMINATED
};

struct Order {
	ServerGameObject *gameObject;
	int id, state = OTS_UNINITIALISED;
	OrderBlueprint *blueprint;
	std::vector<Task*> tasks;
	int nextTaskId = 0;
	int currentTask = -1;

	Order(int id, OrderBlueprint *blueprint, ServerGameObject *gameObject) : id(id), blueprint(blueprint), gameObject(gameObject) {}

	bool isDone() const { return state >= OTS_ABORTED; }
	bool isWorking() const { return (state == OTS_PROCESSING) || (state == OTS_SUSPENDED); }

	void init();
	void start();
	void suspend();
	void resume();
	void cancel();
	void terminate();

	void process();
	Task *getCurrentTask();
	void advanceToNextTask();
};

struct Task {
	Order *order;
	int id, state = OTS_UNINITIALISED;
	TaskBlueprint *blueprint;
	bool firstExecution = true, triggersStarted = false;
	std::vector<Trigger*> triggers;
	bool startSequenceExecuted = false;
	SrvGORef target;
	float proximity = -1.0f; bool proximitySatisfied = false, lastDestinationValid = false;

	Task(int id, TaskBlueprint *blueprint, Order *order);

	bool isDone() const { return state >= OTS_ABORTED; }
	bool isWorking() const { return (state == OTS_PROCESSING) || (state == OTS_SUSPENDED); }

	void init();
	void start();
	void suspend();
	void resume();
	void cancel();
	void terminate();

	void process();
	void startTriggers();
	void stopTriggers();
};

struct Trigger {
	Task *task;
	TriggerBlueprint *blueprint;
	Trigger(Task *task, TriggerBlueprint *blueprint) : task(task), blueprint(blueprint) {}
	virtual void init() {}
	virtual void update() {}
	virtual void parse(GSFileParser &gsf, GameSet &gs);
};

struct TimerTrigger : Trigger {
	float period, referenceTime;
	using Trigger::Trigger;
	void init() override;
	void update() override;
	void parse(GSFileParser &gsf, GameSet &gs) override;
};

struct AnimationLoopTrigger : Trigger {
	float referenceTime;
	using Trigger::Trigger;
	void init() override;
	void update() override;
	void parse(GSFileParser &gsf, GameSet &gs) override;
};


struct OrderConfiguration {
	ServerGameObject *gameobj;
	std::deque<Order> orders;
	int nextOrderId = 0;

	OrderConfiguration(ServerGameObject *gameobj) : gameobj(gameobj) {}
	void process();
	void addOrder(OrderBlueprint *orderBlueprint, int assignMode = 0, ServerGameObject *target = nullptr);
	void cancelAllOrders();

	Order *getCurrentOrder() { return orders.empty() ? nullptr : &orders[0]; }
};
