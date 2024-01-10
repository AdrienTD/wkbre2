// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>
#include <queue>
#include <deque>
#include "gameset/actions.h"
#include "GameObjectRef.h"
#include "util/vecmat.h"

struct OrderBlueprint;
struct TaskBlueprint;
struct TriggerBlueprint;

struct Order;
struct Task;
struct Trigger;

struct ServerGameObject;
struct GameSet;
struct GSFileParser;
struct GameObjBlueprint;

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
	std::vector<std::unique_ptr<Task>> tasks;
	int nextTaskId = 0;
	int currentTask = -1;

	Order(int id, OrderBlueprint *blueprint, ServerGameObject *gameObject) : gameObject(gameObject), id(id), blueprint(blueprint) {}

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
	Order* order;
	int id, state = OTS_UNINITIALISED;
	TaskBlueprint* blueprint;
	bool firstExecution = true, triggersStarted = false;
	std::vector<std::unique_ptr<Trigger>> triggers;
	bool startSequenceExecuted = false;
	SrvGORef target;
	Vector3 destination;
	float proximity = -1.0f; bool proximitySatisfied = false, lastDestinationValid = false;

	Task(int id, TaskBlueprint* blueprint, Order* order);
	virtual ~Task();

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

	void setTarget(ServerGameObject* obj);
	void reevaluateTarget();

	virtual void onStart() {}
	virtual void onUpdate() {}

	static std::unique_ptr<Task> create(int id, TaskBlueprint* blueprint, Order* order);
};

struct MissileTask : Task {
	float msStartTime; Vector3 msInitialPosition, msInitialVelocity;
	using Task::Task;
	virtual void onStart() override;
	virtual void onUpdate() override;
};

struct MoveTask : Task {
	using Task::Task;
	virtual void onStart() override;
	virtual void onUpdate() override;
};

struct ObjectReferenceTask : Task {
	using Task::Task;
	virtual void onStart() override;
	virtual void onUpdate() override;
};

struct FaceTowardsTask : Task {
	using Task::Task;
	//virtual void onStart() override;
	virtual void onUpdate() override;
};

struct SpawnTask : Task {
	GameObjBlueprint* toSpawn = nullptr;
	SrvGORef aiCommissioner; // temp
	using Task::Task;
	virtual void onStart() override;
	virtual void onUpdate() override;
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

struct AttachmentPointTrigger : Trigger {
	uint32_t referenceTime;
	using Trigger::Trigger;
	void init() override;
	void update() override;
	void parse(GSFileParser& gsf, GameSet& gs) override;
};

struct OrderConfiguration {
	ServerGameObject *gameobj;
	std::deque<Order> orders;
	int nextOrderId = 0;
	bool busy = false;

	OrderConfiguration(ServerGameObject *gameobj) : gameobj(gameobj) {}
	void process();
	Order* addOrder(OrderBlueprint *orderBlueprint, int assignMode = 0, ServerGameObject *target = nullptr, const Vector3 &destination = Vector3(-1.0f,-1.0f,-1.0f), bool startNow = true);
	void cancelAllOrders();

	Order *getCurrentOrder() { return orders.empty() ? nullptr : &orders[0]; }
};
