#include "Physics.hpp"
#include "imgui.h"
#include <iostream>
#include <thread>
#include <iostream>

#include "Timer.hpp"

constexpr float ELASTICITY = .6f;
constexpr int IMGUI_FRAME_MARGIN = 4;
constexpr float GRAVITATIONAL_FORCE = 45.f;
constexpr float SPAWNER_EXIT_SPEED = 160.f;
constexpr float MAX_SPEED = SPAWNER_EXIT_SPEED * 3.5f;
constexpr int OBJECT_SIZE = 4;
constexpr int CELL_SIZE = (OBJECT_SIZE * 2);
constexpr int MAX_OBJECTS = 5;
constexpr float DENSITY = 2.f;
constexpr int COLLISION_ITERATIONS = 5;
constexpr int THREAD_COUNT = 4;
constexpr float EPSILON = 0.01;
constexpr float MAX_TIME_STEP(1.f / 60.f);
constexpr glm::vec2 SPAWNER_OFFSET = glm::vec2(-OBJECT_SIZE * 2, OBJECT_SIZE * 2 + 2);



#define USE_COLLISION_GRID
#define USE_THREADS


static uint16_t objCount = 0;

PhysicsController::PhysicsObject::PhysicsObject(PhysicsController* ctrlr, glm::vec2 pos, float r, glm::vec2 v) {
	controller = ctrlr;

	position = pos;
	velocity = v;
	acceleration = glm::vec2(0);
	infrastepTime = 0.f;
	radius = r;	
	mass = r * r * DENSITY;
	
	uint16_t hue = objCount % 360;
	// TODO: turn into interpolation
	double fun = 1 - abs( fmod(static_cast<float>(hue) / 60.f, 2) - 1);
	switch (hue / 60) {
	case 0:
		color = 0xFF0000FF + (static_cast<int>(0xFF * fun) << 8);
		break;
	case 1:
		color = 0xFF00FF00 + static_cast<int>(0xFF * fun);
		break;
	case 2:
		color = 0xFF00FF00 + (static_cast<int>(0xFF * fun) << 16);
		break;
	case 3:
		color = 0xFFFF0000 + (static_cast<int>(0xFF * fun) << 8);
		break;
	case 4:
		color = 0xFFFF0000 + static_cast<int>(0xFF * fun);
		break;
	case 5:
		color = 0xFF0000FF + (static_cast<int>(0xFF * fun) << 16);
		break;
	default:
		color = 0xFFFFFFFF;  
		break;
	}
	objCount++;

}

PhysicsController::PhysicsObject::~PhysicsObject() {
	objCount--;
}

void PhysicsController::PhysicsObject::accelerate(glm::vec2 acc) {
	acceleration += acc;
}


void PhysicsController::PhysicsObject::enforceBoundaries(uint16_t width, uint16_t height) {
	if (position.y > height - radius - IMGUI_FRAME_MARGIN) {
		position.y = height - radius - IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
	}
	if (position.y < radius + IMGUI_FRAME_MARGIN) {
		position.y = radius + IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
	}

	if (position.x > width - radius - IMGUI_FRAME_MARGIN) {
		position.x = width - radius - IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
	}
	if (position.x < radius + IMGUI_FRAME_MARGIN) {
		position.x = radius + IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
	}
}


void PhysicsController::PhysicsObject::update(float timeDelta, uint16_t simWidth, uint16_t simHeight) {
	velocity += acceleration * timeDelta;
	acceleration = glm::vec2(0);
	if (glm::length(velocity) < EPSILON) velocity = glm::vec2(0);
#ifndef USE_QUEUE
	position += velocity * timeDelta;
	enforceBoundaries(simWidth, simHeight);
#endif
}

size_t PhysicsController::CollisionNode::count() const {
	return numObjects;
}

PhysicsController::PhysicsObject* PhysicsController::CollisionNode::getObject(int8_t index) {
	if (index >= numObjects) return nullptr;
	
#ifdef USE_QUEUE
	PhysicsObject* target = head;
	for (int i{ index }; i--;) target = target->next;
	return target;
#else
	return objects[index];
#endif
}

bool PhysicsController::CollisionNode::insert(PhysicsObject* obj) {
	if (!head) {
		head = tail = obj;
		obj->next = obj->previous = obj;
		numObjects++;
		return 1;
	}

	head->previous = tail->next = obj;
	obj->previous = tail;
	obj->next = head;
	tail = obj;
	numObjects++;
	return 1;
}

bool PhysicsController::CollisionNode::remove(PhysicsObject* obj) {
	assert(obj->next);
	if (obj->next == obj) {
		head = 0;
		tail = 0;
		obj->previous = obj->next = 0;
		numObjects--;
		return 1;
	}

	PhysicsObject* current = head;
	do {
		if (current == obj) {
			if (current == head) head = current->next;
			if (current == tail) tail = current->previous;
			current->previous->next = current->next;
			current->next->previous = current->previous;
			current->previous = current->next = 0;
			numObjects--;
			return 1;
		}
 		current = current->next;

	} while (current != head);
	return 0;
}

void PhysicsController::CollisionNode::clear() {
	numObjects = 0;
}


PhysicsController::CollisionGrid::CollisionGrid(uint16_t m, uint16_t n, PhysicsController* ctrlr) : GridContainer<CollisionNode>(m, n, CELL_SIZE) {}


void PhysicsController::CollisionGrid::checkCollision(PhysicsObject* obj1, PhysicsObject* obj2) {
	glm::vec2 distanceVector = obj1->position - obj2->position;
	float dist = glm::length(distanceVector);
	float minDist = obj1->radius + obj2->radius;
	if (dist < minDist) {
		glm::vec2 collisionAxis = distanceVector / dist;
		float delta = minDist - dist;

		glm::vec2 repositionDistance1 = .5f * delta * glm::normalize(collisionAxis);
		glm::vec2 repositionDistance2 = -.5f * delta * glm::normalize(collisionAxis);
		float factorMass1 = 2 * obj2->mass / (obj1->mass + obj2->mass);
		float factorMass2 = 2 * obj1->mass / (obj1->mass + obj2->mass);
		glm::vec2 positionDiffVector = obj1->position - obj2->position;
		glm::vec2 velocityDiffVector = obj1->velocity - obj2->velocity;

		glm::vec2 velocityAdjustment1 = factorMass1 * glm::dot(velocityDiffVector, positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * positionDiffVector;
		glm::vec2 velocityAdjustment2 = factorMass2 * glm::dot(-velocityDiffVector, -positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * -positionDiffVector;

		obj1->position += repositionDistance1;
		obj2->position += repositionDistance2;

		obj1->velocity -= velocityAdjustment1 * ELASTICITY;
		obj2->velocity -= velocityAdjustment2 * ELASTICITY;



		obj1->enforceBoundaries(800, 700);
		obj2->enforceBoundaries(800, 700);
	}
}

void PhysicsController::CollisionGrid::checkCollisionToQueue(PhysicsObject* obj1, PhysicsObject* obj2) {
	// perform quadratic equation to find event time
	glm::vec2 distanceDifference = obj1->position - obj2->position;
	glm::vec2 velocityDifference = obj1->velocity - obj2->velocity;
	float minDistance = obj1->radius + obj2->radius;

	float distanceDifferenceInnerProduct = glm::dot(distanceDifference, distanceDifference);
	float velocityDifferenceInnerProduct = glm::dot(velocityDifference, velocityDifference);
	float bterm = glm::dot(distanceDifference, velocityDifference) / velocityDifferenceInnerProduct;
	float determinate = bterm * bterm - (distanceDifferenceInnerProduct - minDistance * minDistance) / velocityDifferenceInnerProduct;

	// 1 solution if == 0, 2 solutions if >0, no real solutions if <0
	if (determinate >= 0) {
		// we only care about the earliest collision time
		float eventTime = -bterm - sqrt(determinate);
		if (eventTime < 0) return;
		CollisionEvent newEvent = { CollisionEvent::BALL_BALL, eventTime, obj1, NONE, obj2 };
		eventQueue.push(newEvent);
	}
}

void PhysicsController::CollisionGrid::checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2) {
	for (int i = 0; i < cell1->count(); i++) {
		PhysicsObject* obj1 = cell1->getObject(i);
		for (int j = 0; j < cell2->count(); j++) {
			PhysicsObject* obj2 = cell1->getObject(j);
			if (obj1 != obj2) {
#ifdef USE_QUEUE
				checkCollisionToQueue(obj1, obj2);
#else
				checkCollision(obj1, obj2);
#endif
			}
		}
	}
}

void PhysicsController::CollisionGrid::handleCollisions(int widthLow = 1, int widthHigh = -1) {
	//work around since I can't put member variables in default parameters
	if (widthHigh == -1 || widthHigh >= width) widthHigh = width - 1;


	for (int j = 1; j < height - 1; j++) {
		for (int i = widthLow; i < widthHigh; i++) {
			CollisionNode* currentNode = getCell(i, j);
			if (currentNode->count() == 0) continue;
			for (int dj = -1; dj <= 1; dj++) {
				for (int di = -1; di <= 1; di++) {
					CollisionNode* adjacentNode = getCell(i + di, j + dj);
					if (adjacentNode->count() == 0) continue;
					checkCellCollisions(currentNode, adjacentNode);
				}
			}
		}
	}
}

#ifndef USE_QUEUE

void PhysicsController::CollisionGrid::handleCollisionsThreaded(ThreadPool* pool) {
	float step = static_cast<float>(width) / static_cast<float>(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; i++) {
		int widthRangeLow = static_cast<int>(step * i) + 1;
		int widthRangeHigh = static_cast<int>(step * (i + 1)) + 1;
		pool->addTask(std::bind(&CollisionGrid::handleCollisions, this, widthRangeLow, widthRangeHigh));
	}
}

#endif

void PhysicsController::CollisionGrid::addCollisionsToQueue(PhysicsObject* object, float dt) {
	float occuranceTime;

	float eventTime = dt;
	bool eventOccured = false;
	Direction eventDirection = NONE;
	CollisionEvent::CollisionType type = CollisionEvent::ERROR;
	PhysicsObject* predicateObject = 0;


	CollisionNode* currentNode = getCellFromPosition(object->position);
	glm::vec2 newPosition = object->position + object->velocity * (dt - object->infrastepTime);

	// check for cell changes
	if (newPosition.x < currentNode->minimumBound.x) {
		occuranceTime = object->infrastepTime + abs(currentNode->minimumBound.x - object->position.x) / abs(object->velocity.x);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = LEFT;
			type = CollisionEvent::CELL_CHANGE;
		}
	}
	if (newPosition.y < currentNode->minimumBound.y) {
		occuranceTime = object->infrastepTime + abs(currentNode->minimumBound.y - object->position.y) / abs(object->velocity.y);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = UP;
			type = CollisionEvent::CELL_CHANGE;
		}
	}
	if (newPosition.x > currentNode->maximumBound.x) {
		occuranceTime = object->infrastepTime + abs(currentNode->maximumBound.x - object->position.x) / abs(object->velocity.x);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = RIGHT;
			type = CollisionEvent::CELL_CHANGE;
		}
	}
	if (newPosition.y > currentNode->maximumBound.y) {
		occuranceTime = object->infrastepTime + abs(currentNode->maximumBound.y - object->position.y) / abs(object->velocity.y);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = DOWN;
			type = CollisionEvent::CELL_CHANGE;
		}
	}
	 
	// check for boundary enforcements
	if (newPosition.x < object->radius) {
		occuranceTime = object->infrastepTime + abs(object->radius - object->position.x) / abs(object->velocity.x);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = LEFT;
			type = CollisionEvent::BOUNDARY_ENFORCEMENT;
		}
	}
	if (newPosition.y < object->radius) {
		occuranceTime = object->infrastepTime + abs(object->radius - object->position.y) / abs(object->velocity.y);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = UP;
			type = CollisionEvent::BOUNDARY_ENFORCEMENT;
		}
	}
	if (newPosition.x > width * nodeSize - object->radius) {
		occuranceTime = object->infrastepTime + abs(object->position.x - (width * nodeSize - object->radius)) / abs(object->velocity.x);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = RIGHT;
			type = CollisionEvent::BOUNDARY_ENFORCEMENT;
		}
	}
	if (newPosition.y > height * nodeSize - object->radius) {
		occuranceTime = object->infrastepTime + abs(object->position.y - (height * nodeSize - object->radius)) / abs(object->velocity.y);
		if (occuranceTime < eventTime) {
			eventOccured = true;
			eventTime = occuranceTime;
			eventDirection = DOWN;
			type = CollisionEvent::BOUNDARY_ENFORCEMENT;
		}
	}

	// check for object collisions
	for (int dj = -1; dj <= 1; dj++) {
		for (int di = -1; di <= 1; di++) {
			int xAdjacentNodeIndex = currentNode->index.x + di;
			int yAdjacentNodeIndex = currentNode->index.y + dj;
			if (xAdjacentNodeIndex < 0 || yAdjacentNodeIndex < 0 || xAdjacentNodeIndex >= width || yAdjacentNodeIndex >= height) continue;
			CollisionNode* adjacentNode = getCell(xAdjacentNodeIndex, yAdjacentNodeIndex);
			if (adjacentNode->count() == 0) continue;
			for (int i = 0; i < adjacentNode->count(); i++) {
				PhysicsObject* obj2 = adjacentNode->getObject(i);
				if (object == obj2) continue;
				// perform quadratic equation to find event time
				glm::vec2 distanceDifference = object->position - obj2->position;
				glm::vec2 velocityDifference = object->velocity - obj2->velocity;
				float minDistance = object->radius + obj2->radius;

				float distanceDifferenceInnerProduct = glm::dot(distanceDifference, distanceDifference);
				float velocityDifferenceInnerProduct = glm::dot(velocityDifference, velocityDifference);
				float bterm = glm::dot(distanceDifference, velocityDifference) / velocityDifferenceInnerProduct;
				float determinate = (bterm * bterm) - ((distanceDifferenceInnerProduct - minDistance * minDistance) / velocityDifferenceInnerProduct);

				// 1 solution if == 0, 2 solutions if >0, no real solutions if <0
				if (determinate >= 0) {
					// we only care about the earliest collision time
					occuranceTime = object->infrastepTime - bterm - sqrt(determinate);
					if (occuranceTime < eventTime) {
						eventOccured = true;
						type = CollisionEvent::BALL_BALL;
						eventTime = occuranceTime;
						eventDirection = NONE;
						predicateObject = obj2;
					}
				}
			}
		}
	}

	if (eventOccured) {
 		CollisionEvent newEvent = { type, eventTime, object, eventDirection, predicateObject };
		eventQueue.push(newEvent);
	}
}

uint32_t queueCount = 0;
void PhysicsController::CollisionGrid::checkCollisionsQueue(float dt) {
	queueCount = 0;
	CollisionEvent nextCollision;
	CollisionNode* ppp = 0;
	bool skipReentry = false;
	while (!eventQueue.empty()) {
		queueCount++;
		nextCollision = eventQueue.top(); eventQueue.pop();
		switch (nextCollision.type) {
		case CollisionEvent::CELL_CHANGE:
			if (skipReentry) skipReentry = false; continue;
			if (nextCollision.eventTime - nextCollision.subjectObject->infrastepTime == 0) skipReentry = false;

			ppp = getCellFromPosition(nextCollision.subjectObject->position);
			ppp->remove(nextCollision.subjectObject);
			nextCollision.subjectObject->position += nextCollision.subjectObject->velocity * static_cast<float>(nextCollision.eventTime - nextCollision.subjectObject->infrastepTime);
			ppp = getCellFromPosition(nextCollision.subjectObject->position);
			ppp->insert(nextCollision.subjectObject);
			break;

			
		case CollisionEvent::BOUNDARY_ENFORCEMENT:
			nextCollision.subjectObject->position += nextCollision.subjectObject->velocity * static_cast<float>(nextCollision.eventTime - nextCollision.subjectObject->infrastepTime);
			if (nextCollision.eventDirection == UP || nextCollision.eventDirection == DOWN) nextCollision.subjectObject->velocity.y *= -ELASTICITY;
			if (nextCollision.eventDirection == LEFT || nextCollision.eventDirection == RIGHT) nextCollision.subjectObject->velocity.x *= -ELASTICITY;
			nextCollision.subjectObject->velocity *= ELASTICITY;
			break;


		case CollisionEvent::BALL_BALL:
		{
			// move the balls up to the collision point
			glm::vec2 newSubjectPosition = nextCollision.subjectObject->position + nextCollision.subjectObject->velocity * (nextCollision.eventTime - nextCollision.subjectObject->infrastepTime);
			glm::vec2 newPredicatePosition = nextCollision.predicateObject->position + nextCollision.predicateObject->velocity * (nextCollision.eventTime - nextCollision.predicateObject->infrastepTime);

			nextCollision.subjectObject->position = newSubjectPosition;
			nextCollision.predicateObject->position = newPredicatePosition;

			float factorMass1 = 2 * nextCollision.predicateObject->mass / (nextCollision.subjectObject->mass + nextCollision.predicateObject->mass);
			float factorMass2 = 2 * nextCollision.subjectObject->mass / (nextCollision.subjectObject->mass + nextCollision.predicateObject->mass);
			glm::vec2 positionDiffVector = nextCollision.subjectObject->position - nextCollision.predicateObject->position;
			glm::vec2 velocityDiffVector = nextCollision.subjectObject->velocity - nextCollision.predicateObject->velocity;

			glm::vec2 velocityAdjustment1 = factorMass1 * glm::dot(velocityDiffVector, positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * positionDiffVector;
			glm::vec2 velocityAdjustment2 = factorMass2 * glm::dot(-velocityDiffVector, -positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * -positionDiffVector;

			nextCollision.subjectObject->velocity -= velocityAdjustment1 * ELASTICITY;
			nextCollision.predicateObject->velocity -= velocityAdjustment2 * ELASTICITY;

			nextCollision.predicateObject->infrastepTime = nextCollision.eventTime;
		}
			break;
		default:
			std::cerr << "Bad Collision Event Detected\n\tSubject Object ID: " << nextCollision.subjectObject->id 
				<< "\n\tTime of Event: " << nextCollision.eventTime << std::endl;
			exit(1000);
			break;
		}
		nextCollision.subjectObject->infrastepTime = nextCollision.eventTime;
		if (nextCollision.subjectObject->infrastepTime < dt) addCollisionsToQueue(nextCollision.subjectObject, dt);
	}
}


template <typename T>
PhysicsController::ObjectSpawner<T>::ObjectSpawner(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag) {
	controller = ctrlr;
	position = p;
	exitVelocity = mag * dir;
}

template <typename T>
void PhysicsController::ObjectSpawner<T>::shoot(float timeDelta) {
	controller->addObject(new T(controller, position, OBJECT_SIZE, exitVelocity));
}

template <typename T>
void PhysicsController::ObjectSpawner<T>::update(float timeDelta) {
	if (!keepShooting) return;
	timeSinceLastShot += timeDelta;
	if (timeSinceLastShot > REFRACTORY_TIME) {
		shoot(timeDelta);
		timeSinceLastShot -= REFRACTORY_TIME;
	}
}

template <typename T>
void PhysicsController::ObjectSpawner<T>::start() {
	keepShooting = true;
}

template <typename T>
void PhysicsController::ObjectSpawner<T>::stop() {
	keepShooting = false;
}


PhysicsController::PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_) {
	nextID = 0;

	simulationWidth = simulationWidth_;
	simulationHeight = simulationHeight_;

	uint16_t gridWidth = static_cast<uint16_t>(floor(static_cast<float>(simulationWidth) / CELL_SIZE) + 1);
	uint16_t gridHeight = static_cast<uint16_t>(floor(static_cast<float>(simulationHeight) / CELL_SIZE) + 1);

	grid = new CollisionGrid(gridWidth, gridHeight, this);
	pool = new ThreadPool(THREAD_COUNT);

	addSpawnerN({ 75, 75 }, { 1, 0 }, SPAWNER_EXIT_SPEED, 5);
}

PhysicsController::~PhysicsController() {
	for (PhysicsObject* obj : objects) delete obj;
	for (auto spawner : spawners) delete spawner;
	delete grid;
	delete pool;
}

void PhysicsController::addSpawner(glm::vec2 position, glm::vec2 direction, float magnitude) {
	spawners.emplace_back(new ObjectSpawner<PhysicsObject>(this, position + SPAWNER_OFFSET * static_cast<float>(spawners.size()), direction, magnitude));
}

void PhysicsController::addSpawnerN(glm::vec2 p, glm::vec2 dir, float mag, uint8_t n) { 
	for (uint8_t i{ n }; i--;) addSpawner(p, dir, mag); 
}

size_t PhysicsController::getNumObjects() {
	return objects.size();
}

void PhysicsController::addObject(PhysicsObject* obj) {
	objects.push_back(obj);
#ifdef USE_QUEUE
	grid->insert(obj);
#endif
}


void PhysicsController::stopSpawners() {
	for (auto spawner : spawners) spawner->stop();
}
 
void PhysicsController::startSpawners() {
	for (auto spawner : spawners) spawner->start();
}

void PhysicsController::update(float dt) {
	dt = fmin(dt, MAX_TIME_STEP);
	if (objects.size() >= MAX_OBJECTS) { stopSpawners(); }
	for (auto spawner : spawners) {
		spawner->update(dt);
	}
#ifdef USE_QUEUE
	// add all of the objects into the collision queue
	for (auto obj : objects) {
		obj->accelerate(glm::vec2(0, GRAVITATIONAL_FORCE));
		obj->update(dt, simulationWidth, simulationHeight);
		grid->addCollisionsToQueue(obj, dt);
	}

	// go through the queue and run all of the potential collisions
	grid->checkCollisionsQueue(dt);

	// update all objects to the end of the timestep
	for (auto obj : objects) {
		if (obj->infrastepTime != dt) {
			glm::vec2 newPos = obj->position + obj->velocity * (dt - obj->infrastepTime);
			obj->position = newPos;
		}
		// reset infrastepTime for the next frame
		obj->infrastepTime = 0.f;
	}
}
#else

	for (PhysicsObject* obj : objects) {
		obj->accelerate(glm::vec2(0, GRAVITATIONAL_FORCE));
		obj->update(dt, simulationWidth, simulationHeight);
		obj->enforceBoundaries(simulationWidth, simulationHeight);
	}
	handleCollisionsIterations(COLLISION_ITERATIONS);
}

void PhysicsController::handleCollisionsIterations(uint8_t iterations) {
	for (int i{ iterations }; i--;) handleCollisions();
}

void PhysicsController::handleCollisions() {


#ifdef USE_COLLISION_GRID
	grid->clear();
	for (PhysicsObject* obj : objects) {
		assert(grid->insert(obj));
	}

#ifdef USE_THREADS 
	grid->handleCollisionsThreaded(pool);
#else
	grid->handleCollisions();
#endif



#else
	for (PhysicsObject* obj1 : objects) {
		for (PhysicsObject* obj2 : objects) {
			if (obj1 != obj2) {
				grid->checkCollision(obj1, obj2);
			}
		}
	}
#endif
}
#endif



void PhysicsController::displaySimulation() {
	ImGui::SetNextWindowSize({ static_cast<float>(simulationWidth) + IMGUI_FRAME_MARGIN, static_cast<float>(simulationHeight) + IMGUI_FRAME_MARGIN });
	ImGui::SetNextWindowContentSize({ static_cast<float>(simulationWidth), static_cast<float>(simulationHeight) });
	ImGui::Begin("balls", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	for (PhysicsObject* obj : objects) {
		ImVec2 posWindowOffset = ImGui::GetWindowPos();
		ImVec2 posBallOffset = { obj->position.x + posWindowOffset.x, obj->position.y + posWindowOffset.y };
		ImGui::GetWindowDrawList()->AddCircleFilled(posBallOffset, obj->radius, obj->color);
	}

	ImGui::End();
}
