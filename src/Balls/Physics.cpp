#include "Physics.hpp"
#include "imgui.h"
#include <iostream>
#include <thread>
#include <iostream>

constexpr float ELASTICITY = .6f;
constexpr int IMGUI_FRAME_MARGIN = 4;
constexpr float GRAVITATIONAL_FORCE = 45.f;
constexpr float SPAWNER_EXIT_SPEED = 160.f;
constexpr float MAX_SPEED = SPAWNER_EXIT_SPEED * 3.5f;
constexpr int OBJECT_SIZE = 4;
constexpr int CELL_SIZE = (OBJECT_SIZE * 2);
constexpr int MAX_OBJECTS = 2000;
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
#ifndef USE_QUEUE
	position += velocity * timeDelta;
#endif
	acceleration = glm::vec2(0);
	if (glm::length(velocity) < EPSILON) velocity = glm::vec2(0);
	enforceBoundaries(simWidth, simHeight);
}

size_t PhysicsController::CollisionNode::count() const {
	return numObjects;
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

#ifndef USE_QUEUE
void PhysicsController::CollisionGrid::checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2) {
	for (int i = 0; i < cell1->count(); i++) {
		PhysicsObject* obj1 = cell1->objects[i];
		for (int j = 0; j < cell2->count(); j++) {
			PhysicsObject* obj2 = cell2->objects[j];
			if (obj1 != obj2) {
				checkCollision(obj1, obj2);
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
	CollisionEvent newEvent;
	float eventTime;
	float occuranceTime;

	
	glm::vec2 newPosition = object->position + object->velocity * dt;

	// check for cell changes
	CollisionNode* currentNode = getCellFromPosition(object->position);	
	Direction newNodeDirection = NONE;
	
	eventTime = dt;
	bool eventOccured = false;
	if (newPosition.x < currentNode->minimumBound.x) {
		eventOccured = true;
		occuranceTime = (currentNode->minimumBound.x - object->position.x) / object->velocity.x;
		if (occuranceTime < eventTime) {
			eventTime = occuranceTime;
			newNodeDirection = LEFT;
		}
	}
	if (newPosition.y < currentNode->minimumBound.y) {
		eventOccured = true;
		occuranceTime = (currentNode->minimumBound.y - object->position.y) / object->velocity.y;
		if (occuranceTime < eventTime) {
			eventTime = occuranceTime;
			newNodeDirection = UP;
		}
	}
	if (newPosition.x > currentNode->maximumBound.x) {
		eventOccured = true;
		occuranceTime = (object->position.x - currentNode->minimumBound.x) / object->velocity.x;
		if (occuranceTime < eventTime) {
			eventTime = occuranceTime;
			newNodeDirection = RIGHT;
		}
	}
	if (newPosition.y > currentNode->maximumBound.y) {
		eventOccured = true;
		occuranceTime = (object->position.y - currentNode->minimumBound.y) / object->velocity.y;
		if (occuranceTime < eventTime) {
			eventTime = occuranceTime;
			newNodeDirection = DOWN;
		}
	}
	if (eventOccured) newEvent = { CollisionEvent::CELL_CHANGE, eventTime, object, newNodeDirection };
	eventQueue.push(newEvent);
		

	// check for boundary enforcements

	// check for object collisions
}

void PhysicsController::CollisionGrid::checkCollisionsQueue() {
	CollisionEvent nextCollision;
	while (!eventQueue.empty()) {
		nextCollision = eventQueue.top(); eventQueue.pop();
		if (nextCollision.isDirty) continue;
		switch (nextCollision.type) {
		case CollisionEvent::CELL_CHANGE:
			getCellFromPosition(nextCollision.subjectObject->position)->remove(nextCollision.subjectObject);
			nextCollision.subjectObject->position += nextCollision.subjectObject->velocity * static_cast<float>(nextCollision.eventTime - nextCollision.subjectObject->infrastepTime);
			nextCollision.subjectObject->infrastepTime = nextCollision.eventTime;
			getCellFromPosition(nextCollision.subjectObject->position)->insert(nextCollision.subjectObject);
			break;
		default:
			exit(1000);
			break;
		}
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
	simulationWidth = simulationWidth_;
	simulationHeight = simulationHeight_;

	uint16_t gridWidth = static_cast<uint16_t>(floor(static_cast<float>(simulationWidth) / CELL_SIZE) + 3);
	uint16_t gridHeight = static_cast<uint16_t>(floor(static_cast<float>(simulationHeight) / CELL_SIZE) + 3);

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
	grid->checkCollisionsQueue();

	// update all objects to the end of the timestep
	for (auto obj : objects) {
		if (obj->infrastepTime != dt) {
			obj->position += obj->velocity * (dt - obj->infrastepTime);
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
