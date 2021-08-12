#include "Order.h"
#include "tags.h"
#include "gameset/OrderBlueprint.h"
#include "gameset/values.h"
#include "server.h"
#include "util/GSFileParser.h"
#include "gameset/finder.h"
#include "gameset/ScriptContext.h"
#include "gameset/gameset.h"
#include "terrain.h"

void Order::init()
{
	this->blueprint->initSequence.run(this->gameObject);
	for (Task *task : this->tasks)
		task->init();
	this->currentTask = 0;
}

void Order::start()
{
	if (isWorking()) return;
	this->state = OTS_PROCESSING;
	this->currentTask = 0;
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
		this->start();
	if (this->state == OTS_PROCESSING) {
		Task *task = this->getCurrentTask();
		task->process();
	}
}

Task * Order::getCurrentTask()
{
	if (currentTask == -1) return nullptr;
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
		if (trigbp.type == Tags::TASKTRIGGER_TIMER)
			trigger = new TimerTrigger(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_ANIMATION_LOOP)
			trigger = new AnimationLoopTrigger(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_UNINTERRUPTIBLE_ANIMATION_LOOP)
			trigger = new AnimationLoopTrigger(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_ATTACHMENT_POINT)
			trigger = new AnimationLoopTrigger(this, &trigbp); // TODO: Related class
		else
			trigger = new Trigger(this, &trigbp);
		this->triggers[i] = trigger;
	}
}

Task::~Task()
{
	setTarget(nullptr);
}

void Task::init()
{
	this->blueprint->initSequence.run(this->order->gameObject);
}

void Task::start()
{
	if (isWorking()) return;
	this->state = OTS_PROCESSING;
	if (this->blueprint->usePreviousTaskTarget)
		if (this->id > 0)
			this->setTarget(this->order->tasks[this->id - 1]->target.get());
	else if (!this->target && blueprint->taskTarget) {
		SrvScriptContext ctx(Server::instance, this->order->gameObject);
		this->setTarget(blueprint->taskTarget->getFirst(&ctx)); // FIXME: that would override the order's target!!!
	}
	this->startSequenceExecuted = false; // is this correct?

	if (blueprint->classType == Tags::ORDTSKTYPE_MISSILE) {
		SrvScriptContext ssc{ Server::instance, this->order->gameObject };
		float speed = this->order->gameObject->blueprint->missileSpeed->eval(&ssc);

		// compute initial vertical velocity such that the missile hits the target in its trajectory
		Vector3 hvec = (this->target->position - this->order->gameObject->position);
		hvec.y = 0.0f;
		const Vector3 &pos_i = this->order->gameObject->position;
		float y_i = pos_i.y, y_b = this->target->position.y;
		float b = hvec.len2xz() / speed; // time when missile hits target on XZ coordinates
		float vy = (y_b - y_i - 0.5f * (-9.81f) * b * b) / b;

		this->msInitialVelocity = hvec.normal2xz() * speed + Vector3(0, vy, 0);
		this->msInitialPosition = pos_i;
		this->msStartTime = Server::instance->timeManager.currentTime;
		this->order->gameObject->startTrajectory(msInitialPosition, msInitialVelocity, msStartTime);
	}
}

void Task::suspend()
{
	if (!isWorking()) return;
	this->state = OTS_SUSPENDED;
	this->stopTriggers();
	ServerGameObject *go = this->order->gameObject;
	if(go->movement.isMoving())
		go->stopMovement();
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
	ServerGameObject *go = this->order->gameObject;
	if (go->movement.isMoving())
		go->stopMovement();
	this->blueprint->cancellationSequence.run(this->order->gameObject);
}

void Task::terminate()
{
	if (!isWorking()) return;
	this->state = OTS_TERMINATED;
	this->stopTriggers();
	ServerGameObject *go = this->order->gameObject;
	if (go->movement.isMoving())
		go->stopMovement();
	this->blueprint->terminationSequence.run(this->order->gameObject);
	this->order->advanceToNextTask();
}

void Task::process()
{
	if (this->state == OTS_SUSPENDED)
		this->resume();
	else if (this->state != OTS_PROCESSING)
		this->start();
	if (this->state == OTS_PROCESSING) {
		// TODO: Make derived classes of Task to avoid future if elses
		// Missile tasks
		if (this->blueprint->classType == Tags::ORDTSKTYPE_MISSILE) {
			ServerGameObject* go = this->order->gameObject;
			float theight = Server::instance->terrain->getHeight(go->position.x, go->position.z);
			if (go->position.y < theight) {
				// TODO: we could play sound directly on client without sending packet
				// (which is probably why strikeFloorSound was made for WKB in the first place)
				if (go->blueprint->strikeFloorSound != -1) {
					go->playSoundAtObject(go->blueprint->strikeFloorSound, go);
				}
				this->blueprint->struckFloorTrigger.run(go);
			}
			return;
		}
		// Move tasks
		if (this->blueprint->classType == Tags::ORDTSKTYPE_MOVE) {
			ServerGameObject *go = this->order->gameObject;
			if ((go->position - this->destination).sqlen2xz() < 0.1f) {
				terminate();
			}
			if (!go->movement.isMoving())
				go->startMovement(this->destination);
			return;
		}
		// Target tasks
		if (!this->target) {
			this->stopTriggers();
			ServerGameObject* go = this->order->gameObject;
			if (go->movement.isMoving())
				go->stopMovement();
			if (blueprint->taskTarget) {
				SrvScriptContext ctx(Server::instance, this->order->gameObject);
				this->setTarget(blueprint->taskTarget->getFirst(&ctx));
			}
		}
		if (this->target) {
			if (!this->startSequenceExecuted) {
				this->blueprint->startSequence.run(order->gameObject);
				this->startSequenceExecuted = true;
				if (this->blueprint->proximityRequirement) {
					SrvScriptContext ctx(Server::instance, this->order->gameObject);
					this->proximity = this->blueprint->proximityRequirement->eval(&ctx); // should this be here?
				}
			}
			ServerGameObject *go = this->order->gameObject;
			if (this->proximity < 0.0f || (go->position - this->target->position).sqlen2xz() < this->proximity * this->proximity) {
				this->startTriggers();
				if (go->movement.isMoving())
					go->stopMovement();
				if (!proximitySatisfied) {
					proximitySatisfied = true;
					blueprint->proximitySatisfiedSequence.run(order->gameObject);
					//
					int animtoplay = -1;
					for (const auto& ea : blueprint->equAnims) {
						SrvScriptContext ctx(Server::instance, this->order->gameObject);
						if (Server::instance->gameSet->equations[ea.first]->booleval(&ctx)) {
							animtoplay = ea.second;
							break;
						}
					}
					if (animtoplay == -1)
						animtoplay = blueprint->defaultAnim;
					if (animtoplay != -1)
						go->setAnimation(animtoplay);
				}
			}
			else {
				this->stopTriggers();
				if (!go->movement.isMoving())
					go->startMovement(this->target->position);
				// If target slightly moves during the movement, restart the movement
				if (go->movement.isMoving())
					if ((this->target->position - go->movement.getDestination()).sqlen2xz() > 0.01f)
						go->startMovement(this->target->position);
				if (proximitySatisfied) {
					proximitySatisfied = false;
					//if (blueprint->defaultAnim != -1)
					//	go->setAnimation(0);
				}
			}
			if (this->triggersStarted) {
				for (Trigger *trigger : this->triggers)
					trigger->update();
			}
		}
		else {
			terminate();
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

void Task::setTarget(ServerGameObject* newtarget)
{
	ServerGameObject* oldtarget = target.get();
	//if (oldtarget == newtarget)
	//	return;
	// potential problem: if oldtarget was on deleted obj, and newtarget is null, the equality above will also turn true, leaving oldtarget on deleted obj
	// only becomes problem when suddenly new object is created with the deleted object's ID, which should "rarely" happen
	if (oldtarget) {
		std::swap(*std::find(oldtarget->referencers.begin(), oldtarget->referencers.end(), this->order->gameObject), oldtarget->referencers.back());
		oldtarget->referencers.pop_back();
	}
	target = newtarget;
	if (newtarget)
		newtarget->referencers.push_back(this->order->gameObject);
}



void OrderConfiguration::addOrder(OrderBlueprint * orderBlueprint, int assignMode, ServerGameObject *target, const Vector3 &destination)
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
		neworder->tasks.at(0)->setTarget(target);
	}
	if (destination.x >= 0.0f) {
		neworder->tasks.at(0)->destination = destination;
	}
	neworder->init();
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
	SrvScriptContext ctx(Server::instance, this->task->order->gameObject);
	period = this->blueprint->period->eval(&ctx);
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

void AnimationLoopTrigger::init()
{
	referenceTime = Server::instance->timeManager.currentTime;
}

void AnimationLoopTrigger::update()
{
	ServerGameObject* obj = this->task->order->gameObject;
	Model* model = obj->blueprint->getModel(obj->subtype, obj->appearance, obj->animationIndex, obj->animationVariant);
	float period = model ? model->getDuration() : 1.5f;
	if (Server::instance->timeManager.currentTime >= referenceTime + period) {
		this->blueprint->actions.run(this->task->order->gameObject);
		this->referenceTime += period;
	}
}

void AnimationLoopTrigger::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string word = gsf.nextTag();
		if (word == "REFERENCE_TIME")
			referenceTime = (float)gsf.nextInt() / 1000.0f;
		else if (word == "END_TRIGGER")
			break;
		gsf.advanceLine();
	}
}
