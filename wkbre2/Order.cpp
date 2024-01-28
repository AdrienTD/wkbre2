// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
#include "NNSearch.h"

void Order::init()
{
	for (auto& task : this->tasks)
		task->init();
	this->currentTask = 0;
}

void Order::start()
{
	if (isWorking()) return;
	this->state = OTS_PROCESSING;
	this->currentTask = 0;
	this->blueprint->initSequence.run(this->gameObject);
	this->getCurrentTask()->start();
	this->blueprint->startSequence.run(this->gameObject);
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
	this->tasks[this->currentTask]->resume();
	this->blueprint->resumptionSequence.run(this->gameObject);
}

void Order::cancel()
{
	if (isDone()) return;
	this->state = OTS_CANCELLED;
	this->tasks[this->currentTask]->cancel();
	this->blueprint->cancellationSequence.run(this->gameObject);
	this->gameObject->updateBuildingOrderCount(this->blueprint);
}

void Order::terminate()
{
	if (isDone()) return;
	this->state = OTS_TERMINATED;
	this->tasks[this->currentTask]->terminate();
	this->blueprint->terminationSequence.run(this->gameObject);
	this->gameObject->updateBuildingOrderCount(this->blueprint);
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
	return this->tasks[this->currentTask].get();
}

void Order::advanceToNextTask()
{
	currentTask = (currentTask + 1) % tasks.size();
	if (currentTask == 0 && !blueprint->cycleOrder) {
		terminate();
	}
}

Task::Task(int id, TaskBlueprint * blueprint, Order * order) : order(order), id(id), blueprint(blueprint)
{
	this->triggers.resize(blueprint->triggers.size());
	for (size_t i = 0; i < this->triggers.size(); i++) {
		auto &trigbp = blueprint->triggers[i];
		std::unique_ptr<Trigger> trigger;
		if (trigbp.type == Tags::TASKTRIGGER_TIMER)
			trigger = std::make_unique<TimerTrigger>(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_ANIMATION_LOOP)
			trigger = std::make_unique<AnimationLoopTrigger>(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_UNINTERRUPTIBLE_ANIMATION_LOOP)
			trigger = std::make_unique<AnimationLoopTrigger>(this, &trigbp);
		else if (trigbp.type == Tags::TASKTRIGGER_ATTACHMENT_POINT)
			trigger = std::make_unique<AttachmentPointTrigger>(this, &trigbp);
		else
			trigger = std::make_unique<Trigger>(this, &trigbp);
		this->triggers[i] = std::move(trigger);
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
	if (this->blueprint->usePreviousTaskTarget) {
		if (this->id > 0)
			this->setTarget(this->order->tasks[this->id - 1]->target.get());
	}
	else if (!this->target && blueprint->taskTarget) {
		SrvScriptContext ctx(Server::instance, this->order->gameObject);
		this->setTarget(blueprint->taskTarget->getFirst(&ctx)); // FIXME: that would override the order's target!!!
	}
	this->startSequenceExecuted = false; // is this correct?
	if (this->target) {
		if (!this->startSequenceExecuted) {
			this->blueprint->startSequence.run(order->gameObject);
			this->startSequenceExecuted = true;
			if (this->blueprint->proximityRequirement) {
				SrvScriptContext ctx(Server::instance, this->order->gameObject);
				this->proximity = this->blueprint->proximityRequirement->eval(&ctx); // should this be here?
			}
		}
	}
	this->onStart();
}

void Task::suspend()
{
	if (!isWorking()) return;
	this->state = OTS_SUSPENDED;
	this->stopTriggers();
	ServerGameObject *go = this->order->gameObject;
	if(go->movementController.isMoving())
		go->movementController.stopMovement();
	go->setAnimation(0);
	//this->blueprint->suspensionSequence.run(this->order->gameObject);
}

void Task::resume()
{
	this->state = OTS_PROCESSING;
	this->blueprint->resumptionSequence.run(this->order->gameObject);
	this->proximitySatisfied = false; // TODO: put this in "onResume"
}

void Task::cancel()
{
	if (isDone()) return;
	this->state = OTS_CANCELLED;
	this->stopTriggers();
	ServerGameObject *go = this->order->gameObject;
	if (go->movementController.isMoving())
		go->movementController.stopMovement();
	go->setAnimation(0);
	this->blueprint->cancellationSequence.run(this->order->gameObject);
}

void Task::terminate()
{
	if (isDone()) return;
	this->state = OTS_TERMINATED;
	this->stopTriggers();
	ServerGameObject *go = this->order->gameObject;
	if (go->movementController.isMoving())
		go->movementController.stopMovement();
	go->setAnimation(0);
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
		this->onUpdate();
	}
}

void Task::startTriggers()
{
	if (triggersStarted)
		return;
	triggersStarted = true;
	for (auto& trigger : triggers)
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

void Task::reevaluateTarget()
{
	suspend();
	if (blueprint->taskTarget) {
		SrvScriptContext ctx(Server::instance, this->order->gameObject);
		this->setTarget(blueprint->taskTarget->getFirst(&ctx));
	}
}

std::unique_ptr<Task> Task::create(int id, TaskBlueprint* blueprint, Order* order)
{
	std::unique_ptr<Task> task;
	switch (blueprint->classType) {
	default:
	case Tags::ORDTSKTYPE_OBJECT_REFERENCE: task = std::make_unique<ObjectReferenceTask>(id, blueprint, order); break;
	case Tags::ORDTSKTYPE_MOVE: task = std::make_unique<MoveTask>(id, blueprint, order); break;
	case Tags::ORDTSKTYPE_MISSILE: task = std::make_unique<MissileTask>(id, blueprint, order); break;
	case Tags::ORDTSKTYPE_FACE_TOWARDS: task = std::make_unique<FaceTowardsTask>(id, blueprint, order); break;
	case Tags::ORDTSKTYPE_SPAWN: task = std::make_unique<SpawnTask>(id, blueprint, order); break;
	}
	return task;
}



Order* OrderConfiguration::addOrder(OrderBlueprint * orderBlueprint, int assignMode, ServerGameObject *target, const Vector3 &destination, bool startNow)
{
	if (Order* currentOrder = getCurrentOrder()) {
		if (currentOrder->blueprint->cannotInterruptOrder && assignMode != Tags::ORDERASSIGNMODE_DO_LAST)
			return nullptr;
	}
	Order* neworder;
	bool orderInFront = false;
	switch (assignMode) {
	case Tags::ORDERASSIGNMODE_DO_FIRST:
		if (!this->orders.empty()) {
			this->orders.front().suspend();
		}
		this->orders.emplace_front(this->nextOrderId++, orderBlueprint, gameobj);
		neworder = &this->orders.front();
		orderInFront = true;
		break;
	case Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE:
		this->cancelAllOrders();
		orderInFront = true;
		[[fallthrough]];
	default:
	case Tags::ORDERASSIGNMODE_DO_LAST:
		// should orderInFront be set to true if no other orders present?
		// if DO_LAST is used, it should be expected that the order cannot start immediately
		// so let's keep it false for now
		this->orders.emplace_back(this->nextOrderId++, orderBlueprint, gameobj);
		neworder = &this->orders.back();
		break;
	}
	for (TaskBlueprint* taskBp : orderBlueprint->tasks) {
		int id = neworder->nextTaskId++;
		neworder->tasks.push_back(Task::create(id, taskBp, neworder));
	}
	if (target) {
		neworder->tasks.at(0)->setTarget(target);
	}
	if (destination.x >= 0.0f) {
		neworder->tasks.at(0)->destination = destination;
	}
	neworder->init();
	if (orderInFront && startNow) // start order (and first task) immediately if no other working order behind
		neworder->start();
	this->gameobj->updateBuildingOrderCount(orderBlueprint);
	return neworder;
}

void OrderConfiguration::cancelAllOrders()
{
	// we cannot delete the orders yet, they might still be referenced
	size_t count = orders.size(); // copy of the size, so that if new orders are created during the cancelling sequence, it won't cancel the new ones
	for (size_t i = 0; i < count; ++i) {
		orders[i].cancel();
	}
	gameobj->reportCurrentOrder(nullptr);
}

void OrderConfiguration::process()
{
	while (!orders.empty() && orders.front().isDone()) {
		orders.pop_front();
	}
	if (!orders.empty()) {
		orders.front().process();
	}
	bool nextBusy = !orders.empty();
	if (busy && !nextBusy)
		gameobj->sendEvent(Tags::PDEVENT_ON_IDLE);
	busy = nextBusy;

	Order* curorder = getCurrentOrder();
	gameobj->reportCurrentOrder(curorder ? curorder->blueprint : nullptr);
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
	if (obj->animSynchronizedTask != -1) {
		// TODO: We might need to prevent the trigger to happen twice if the task is not terminated in the trigger sequence
		// The Construction Animation tasks do terminate the order, so it's not "that" important.
		SrvScriptContext ctx{ Server::instance, obj };
		float frac = Server::instance->gameSet->tasks[obj->animSynchronizedTask].synchAnimationToFraction->eval(&ctx);
		if (frac >= 1.0f) {
			this->blueprint->actions.run(this->task->order->gameObject);
		}
	}
	else {
		float period = model ? model->getDuration() : 1.5f;
		if (Server::instance->timeManager.currentTime >= referenceTime + period) {
			this->blueprint->actions.run(this->task->order->gameObject);
			this->referenceTime += period;
		}
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

void AttachmentPointTrigger::init()
{
	referenceTime = (uint32_t)(Server::instance->timeManager.currentTime * 1000.0f);
}

void AttachmentPointTrigger::update()
{
	ServerGameObject* obj = this->task->order->gameObject;
	Model* model = obj->blueprint->getModel(obj->subtype, obj->appearance, obj->animationIndex, obj->animationVariant);
	if (model) {
		uint32_t startTime = (uint32_t)(obj->animStartTime * 1000.0f);
		uint32_t prevTime = (uint32_t)(Server::instance->timeManager.previousTime * 1000.0f) - startTime;
		uint32_t nextTime = (uint32_t)(Server::instance->timeManager.currentTime * 1000.0f) - startTime;
		size_t numAPs = model->getNumAPs();
		for (size_t i = 0; i < numAPs; i++) {
			if (model->getAPInfo(i).tag.substr(0, 3) == "SP_") {
				auto p = model->hasAPFlagSwitched(i, prevTime, nextTime);
				if (p.first && p.second) {
					blueprint->actions.run(obj);
					break;
				}
			}
		}
	}
}

void AttachmentPointTrigger::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string word = gsf.nextTag();
		if (word == "REFERENCE_TIME")
			referenceTime = gsf.nextInt();
		else if (word == "END_TRIGGER")
			break;
		gsf.advanceLine();
	}
}

void MissileTask::onStart()
{
	// remove the missile object if it has no target
	if (!this->target) {
		Server::instance->deleteObject(this->order->gameObject);
		return;
	}

	SrvScriptContext ssc{ Server::instance, this->order->gameObject };
	float speed = this->order->gameObject->blueprint->missileSpeed->eval(&ssc);

	// missile destination to hit
	Vector3 dest = this->target->position;
	if (Model* model = this->target->blueprint->getModel(target->subtype, target->appearance, target->animationIndex, target->animationVariant)) {
		Vector3 center = model->getSphereCenter().transform(target->getWorldMatrix());
		if (center.y < dest.y)
			center.y = dest.y;
		dest = center;
	}
	// add target's movement into account
	if (this->target->movement.isMoving()) {
		// Note: This is not perfect, as it kind of assumes that the time the missile will take to hit
		// the adjusted position won't change, which is not always true. For slow target movement this
		// might be ok, but faster ones not so much.
		Vector3 hvec = (dest - this->order->gameObject->position);
		float b = hvec.len2xz() / speed; // time when missile hits target on XZ coordinates (on original position!!!)
		dest += this->target->movement.getNewPosition(Server::instance->timeManager.currentTime + b) - this->target->position;
	}

	// compute initial vertical velocity such that the missile hits the target in its trajectory
	Vector3 hvec = (dest - this->order->gameObject->position);
	hvec.y = 0.0f;
	const Vector3& pos_i = this->order->gameObject->position;
	float y_i = pos_i.y, y_b = dest.y;
	float b = hvec.len2xz() / speed; // time when missile hits target on XZ coordinates
	float vy = (y_b - y_i - 0.5f * (-9.81f) * b * b) / b;

	this->msInitialVelocity = hvec.normal2xz() * speed + Vector3(0, vy, 0);
	this->msInitialPosition = pos_i;
	this->msStartTime = Server::instance->timeManager.currentTime;
	this->order->gameObject->startTrajectory(msInitialPosition, msInitialVelocity, msStartTime);
	this->msPrevPosition = this->msInitialPosition;
}

static bool LineSegmentIntersectsSphere(const Vector3& lineStart, const Vector3& lineEnd, const Vector3& center, float radius)
{
	float lineLength = (lineEnd - lineStart).len3();
	Vector3 lineDir = (lineEnd - lineStart).normal();
	Vector3 smc = lineStart - center;
	float ddt = lineDir.dot(smc);
	float delta = ddt * ddt - (smc.sqlen3() - radius * radius);
	if (delta < 0.0f)
		return false;
	float k1 = -ddt + sqrtf(delta),
		k2 = -ddt - sqrtf(delta);
	if ((k1 >= 0.0f && k1 <= lineLength) || (k2 >= 0.0f && k2 <= lineLength))
		return true;
	return false;
}

void MissileTask::onUpdate()
{
	ServerGameObject* go = this->order->gameObject;

	Vector3 prevPosition = msPrevPosition;
	msPrevPosition = go->position;

	// unit collision
	NNSearch nns;
	nns.start(Server::instance, go->position, 15.0f);
	while (ServerGameObject* col = (ServerGameObject*)nns.next()) {
		if (col->blueprint->bpClass == Tags::GAMEOBJCLASS_BUILDING || col->blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER) {
			if (col->getPlayer() != go->getPlayer() && col->isInteractable()) {
				if (Model* model = col->blueprint->getModel(col->subtype, col->appearance, col->animationIndex, col->animationVariant)) {
					Vector3 sCenter = model->getSphereCenter().transform(col->getWorldMatrix());
					float sRadius = model->getSphereRadius();
					if (LineSegmentIntersectsSphere(prevPosition, go->position, sCenter, sRadius)) {
						SrvScriptContext ctx(Server::instance, go);
						auto _ = ctx.change(ctx.collisionSubject, col);
						this->blueprint->collisionTrigger.run(&ctx);
						return;
					}
				}
			}
		}
	}

	// floor collision
	float theight = Server::instance->terrain->getHeight(go->position.x, go->position.z);
	if (go->position.y < theight) {
		// TODO: we could play sound directly on client without sending packet
		// (which is probably why strikeFloorSound was made for WKB in the first place)
		if (go->blueprint->strikeFloorSound != -1) {
			go->playSoundAtObject(go->blueprint->strikeFloorSound, go);
		}
		this->blueprint->struckFloorTrigger.run(go);
	}
}

void MoveTask::onStart()
{
}

void MoveTask::onUpdate()
{
	ServerGameObject* go = this->order->gameObject;
	const Vector3& dest = this->target ? this->target->position : this->destination;
	if ((go->position - dest).sqlen2xz() < 0.1f) {
		terminate();
	}
	if (!go->movementController.isMoving())
		destination = go->movementController.startMovement(dest);
}

void ObjectReferenceTask::onStart()
{
	this->proximitySatisfied = false; // might need to do this on resumption too
	if (target)
		this->destination = target->position;
}

void ObjectReferenceTask::onUpdate()
{
	if (!this->target) {
		this->stopTriggers();
		ServerGameObject* go = this->order->gameObject;
		if (go->movementController.isMoving())
			go->movementController.stopMovement();
		if (blueprint->taskTarget) {
			SrvScriptContext ctx(Server::instance, this->order->gameObject);
			this->setTarget(blueprint->taskTarget->getFirst(&ctx));
		}
	}
	if (blueprint->rejectTargetIfItIsTerminated) {
		if (this->target && (this->target->flags & ServerGameObject::fTerminated)) {
			this->setTarget(nullptr);
			ServerGameObject* go = this->order->gameObject;
			if (go->movementController.isMoving())
				go->movementController.stopMovement();
		}
	}
	if (this->target) {
		ServerGameObject* go = this->order->gameObject;
		float prox = this->proximity;
		if (prox >= 0.0f) prox = std::max(prox - (destination - target->position).len2xz(), 0.1f);
		if (prox < 0.0f || (go->position - destination).sqlen2xz() < prox * prox) {
			this->startTriggers();
			if (go->movementController.isMoving())
				go->movementController.stopMovement();
			if (!proximitySatisfied) {
				proximitySatisfied = true;
				blueprint->proximitySatisfiedSequence.run(order->gameObject);
				blueprint->movementCompletedSequence.run(order->gameObject);

				// Set the task animation
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
					go->setAnimation(animtoplay, blueprint->playAnimationOnce, blueprint->synchAnimationToFraction ? blueprint->bpid : -1);

				// Set orientation towards target, TODO: update when target moves (perhaps on Client?)
				if (go->blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER) {
					Vector3 dir = (target->position - go->position).normal2xz();
					if (!(dir.x == 0.0f && dir.z == 0.0f))
						go->setOrientation(Vector3(0, std::atan2(dir.x, -dir.z), 0));
				}
			}
		}
		else {
			this->stopTriggers();
			if (!go->movementController.isMoving())
				destination = go->movementController.startMovement(this->target->position);
			// If target slightly moves during the movement, restart the movement
			if (target->blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER)
				if (go->movementController.isMoving())
					if ((this->target->position - go->movementController.getDestination()).sqlen2xz() > 0.1f)
						destination = go->movementController.startMovement(this->target->position);
			if (proximitySatisfied) {
				proximitySatisfied = false;
				//if (blueprint->defaultAnim != -1)
				//	go->setAnimation(0);
			}
		}
		if (this->triggersStarted) {
			for (auto& trigger : this->triggers)
				trigger->update();
		}
	}
	else {
		terminate();
		if (blueprint->terminateEntireOrderIfNoTarget)
			order->terminate();
	}
}

void FaceTowardsTask::onUpdate()
{
	if (target) {
		ServerGameObject* obj = this->order->gameObject;
		Vector3 dir = target->position - obj->position;
		obj->setOrientation(Vector3(0.0f, std::atan2(dir.x, -dir.z), 0.0f));
	}
	terminate();
}

void SpawnTask::onStart()
{
	ServerGameObject* obj = order->gameObject;
	obj->setItem(Tags::PDITEM_HIT_POINTS_OF_OBJECT_BEING_SPAWNED, 0.0f);
	obj->setItem(Tags::PDITEM_HIT_POINT_CAPACITY_OF_OBJECT_BEING_SPAWNED, toSpawn->startItemValues.at(Tags::PDITEM_HIT_POINT_CAPACITY));
}

void SpawnTask::onUpdate()
{
	this->startTriggers();
	if (this->triggersStarted) {
		for (auto& trigger : this->triggers)
			trigger->update();
	}
	if (this->isDone()) {
		ServerGameObject* obj = order->gameObject;
		if (obj->getItem(Tags::PDITEM_HIT_POINTS_OF_OBJECT_BEING_SPAWNED) >= obj->getItem(Tags::PDITEM_HIT_POINT_CAPACITY_OF_OBJECT_BEING_SPAWNED)) {
			ServerGameObject* spawned = Server::instance->spawnObject(toSpawn, obj->getPlayer(), obj->position, Vector3(0,0,0));
			if (ServerGameObject* com = aiCommissioner.get())
				com->sendEvent(Tags::PDEVENT_ON_COMMISSIONED, spawned);
		}
	}
}
