#include "Order.h"
#include "tags.h"
#include "gameset/OrderBlueprint.h"
#include "gameset/values.h"
#include "server.h"
#include "util/GSFileParser.h"

void Order::init()
{
	if (isWorking()) return;
	this->state = OTS_PROCESSING;
	this->currentTask = 0;
	this->blueprint->initSequence.run(this->gameObject);
}

void Order::suspend()
{
	if (!isWorking()) return;
	this->state = OTS_SUSPENDED;
	this->tasks[this->currentTask]->suspend();
	//this->blueprint->suspensionSequence.run(this->gameObject);
}

void Order::resume()
{
	this->state = OTS_PROCESSING;
	this->blueprint->resumptionSequence.run(this->gameObject);
}

void Order::cancel()
{
	if (!isWorking()) return;
	this->state = OTS_CANCELLED;
	this->tasks[this->currentTask]->cancel();
	this->blueprint->cancellationSequence.run(this->gameObject);
}

void Order::terminate()
{
	if (!isWorking()) return;
	this->state = OTS_TERMINATED;
	this->tasks[this->currentTask]->terminate();
	this->blueprint->terminationSequence.run(this->gameObject);
}

void Order::process()
{
	if (this->state == OTS_SUSPENDED)
		this->resume();
	else if (this->state != OTS_PROCESSING)
		this->init();
	if (this->state == OTS_PROCESSING) {
		Task *task = this->getCurrentTask();
		task->process();
	}
}

Task * Order::getCurrentTask()
{
	return this->tasks[this->currentTask];
}

void Order::advanceToNextTask()
{
	currentTask = (currentTask + 1) % tasks.size();
	if (currentTask == 0 && !blueprint->cycleOrder) {
		terminate();
	}
}

Task::Task(int id, TaskBlueprint * blueprint, Order * order) : id(id), blueprint(blueprint), order(order)
{
	this->triggers.resize(blueprint->triggers.size());
	for (int i = 0; i < this->triggers.size(); i++) {
		auto &trigbp = blueprint->triggers[i];
		Trigger *trigger;
		if (trigbp.type = Tags::TASKTRIGGER_TIMER)
			trigger = new TimerTrigger(this, &trigbp);
		else
			trigger = new Trigger(this, &trigbp);
		this->triggers[i] = trigger;
	}
}

void Task::init()
{
	if (isWorking()) return;
	this->state = OTS_PROCESSING;
	this->blueprint->initSequence.run(this->order->gameObject);
}

void Task::suspend()
{
	if (!isWorking()) return;
	this->state = OTS_SUSPENDED;
	this->stopTriggers();
	//this->blueprint->suspensionSequence.run(this->order->gameObject);
}

void Task::resume()
{
	this->state = OTS_PROCESSING;
	this->blueprint->resumptionSequence.run(this->order->gameObject);
}

void Task::cancel()
{
	if (!isWorking()) return;
	this->state = OTS_CANCELLED;
	this->stopTriggers();
	this->blueprint->cancellationSequence.run(this->order->gameObject);
}

void Task::terminate()
{
	if (!isWorking()) return;
	this->state = OTS_TERMINATED;
	this->stopTriggers();
	this->blueprint->terminationSequence.run(this->order->gameObject);
	this->order->advanceToNextTask();
}

void Task::process()
{
	if (this->state == OTS_SUSPENDED)
		this->resume();
	else if (this->state != OTS_PROCESSING)
		this->init();
	if (this->state == OTS_PROCESSING) {
		if (!this->startSequenceExecuted) {
			this->blueprint->startSequence.run(order->gameObject);
			this->startSequenceExecuted = true;
		}
		this->startTriggers(); // suppose the proximity is always satisfied for now
		if (this->triggersStarted) {
			for (Trigger *trigger : this->triggers)
				trigger->update();
		}
	}
}

void Task::startTriggers()
{
	if (triggersStarted)
		return;
	triggersStarted = true;
	for (Trigger *trigger : triggers)
		trigger->init();
}

void Task::stopTriggers()
{
	triggersStarted = false;
}



void OrderConfiguration::addOrder(OrderBlueprint * orderBlueprint, int assignMode, ServerGameObject *target)
{
	Order *neworder;
	switch (assignMode) {
	case Tags::ORDERASSIGNMODE_DO_FIRST:
		if (!this->orders.empty()) {
			this->orders.front().suspend();
		}
		this->orders.emplace_front(this->nextOrderId++, orderBlueprint, gameobj);
		neworder = &this->orders.front();
		break;
	case Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE:
		this->cancelAllOrders();
	default:
	case Tags::ORDERASSIGNMODE_DO_LAST:
		this->orders.emplace_back(this->nextOrderId++, orderBlueprint, gameobj);
		neworder = &this->orders.back();
		break;
	}
	for (TaskBlueprint *taskBp : orderBlueprint->tasks)
		neworder->tasks.push_back(new Task(neworder->nextTaskId++, taskBp, neworder));
	if (target) {
		neworder->tasks.at(0)->target = target;
	}
}

void OrderConfiguration::cancelAllOrders()
{
	while (!orders.empty()) {
		orders.front().cancel();
		orders.pop_front();
	}
}

void OrderConfiguration::process()
{
	while (!orders.empty() && orders.front().isDone()) {
		orders.pop_front();
	}
	if (!orders.empty()) {
		orders.front().process();
	}
}

void Trigger::parse(GSFileParser &gsf, GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string word = gsf.nextTag();
		if (word == "END_TRIGGER")
			break;
		gsf.advanceLine();
	}
}

void TimerTrigger::init()
{
	period = this->blueprint->period->eval(this->task->order->gameObject);
	referenceTime = Server::instance->timeManager.currentTime;
}

void TimerTrigger::update()
{
	if (Server::instance->timeManager.currentTime >= referenceTime + period) {
		this->blueprint->actions.run(this->task->order->gameObject);
		//TimerTrigger::init();
		this->referenceTime += this->period;
	}
}

void TimerTrigger::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string word = gsf.nextTag();
		if (word == "PERIOD")
			period = gsf.nextFloat();
		else if (word == "REFERENCE_TIME")
			referenceTime = gsf.nextFloat();
		else if (word == "END_TRIGGER")
			break;
		gsf.advanceLine();
	}
}
