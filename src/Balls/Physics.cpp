#include "Physics.hpp"
#include "imgui.h"
#include <thread>
#include <iostream>

#define ELASTICITY .6f
#define IMGUI_FRAME_MARGIN 4
#define GRAVITAIONAL_FORCE 140
#define SPAWNER_EXIT_SPEED 260
#define OBJECT_SIZE 8
#define CELL_SIZE (OBJECT_SIZE * 2)
#define MAX_OBJECTS 2500
#define SPAWNER_OFFSET glm::vec2(-8, OBJECT_SIZE * 2 + 2)
#define DENSITY 2
#define COLLISION_ITERATIONS 8
#define THREAD_COUNT 16
#define MAX_TIME_STEP (1.f / 60.f)

#define BASE_COLOR 0xffffffff


#define USE_COLLISION_GRID
//#define USE_THREAD


class PhysicsController;
 
PhysicsObject::PhysicsObject(glm::vec2 pos, float r, glm::vec2 v, uint32_t col = BASE_COLOR) {
	position = pos;
	velocity = v;
	acceleration = glm::vec2(0);
	radius = r;
	color = col;
	mass = r * r * DENSITY;
}

void PhysicsObject::accelerate(glm::vec2 acc) {
	acceleration += acc;
}

void PhysicsObject::addNormalDirection(glm::vec2 dir) {
	dir /= glm::length(dir);
	normalForce += dir;
	normalForce /= glm::length(normalForce);
}

void PhysicsObject::enforceBoundaries(uint16_t width, uint16_t height) {
	if (position.y >= height - radius - IMGUI_FRAME_MARGIN) {
		position.y = height - radius - IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
		addNormalDirection(glm::vec2(0, -1));
	}
	if (position.y <= radius + IMGUI_FRAME_MARGIN) {
		position.y = radius + IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
		addNormalDirection(glm::vec2(0, 1));
	}

	if (position.x >= width - radius - IMGUI_FRAME_MARGIN) {
		position.x = width - radius - IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
		addNormalDirection(glm::vec2(-1, 0));
	}
	if (position.x <= radius + IMGUI_FRAME_MARGIN) {
		position.x = radius + IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
		addNormalDirection(glm::vec2(1, 0));
	}
}

void PhysicsObject::findNormal(uint16_t width, uint16_t height) {
	if (position.y >= height - radius - IMGUI_FRAME_MARGIN) {
		addNormalDirection(glm::vec2(0, -1));
	}
	if (position.y <= radius + IMGUI_FRAME_MARGIN) {
		addNormalDirection(glm::vec2(0, 1));
	}

	if (position.x >= width - radius - IMGUI_FRAME_MARGIN) {
		addNormalDirection(glm::vec2(-1, 0));
	}
	if (position.x <= radius + IMGUI_FRAME_MARGIN) {
		addNormalDirection(glm::vec2(1, 0));
	}
}

void PhysicsObject::update(float timeDelta, uint16_t simWidth, uint16_t simHeight) {
	normalForce = glm::vec2(0);
	findNormal(simWidth, simHeight);
	velocity += acceleration * timeDelta;
	position += velocity * timeDelta;
	acceleration = glm::vec2(0);
}

size_t CollisionNode::count() const {
	return numObjects;
}

bool CollisionNode::insert(PhysicsObject* obj) {
	assert(numObjects < maxObjects);
	if (numObjects >= maxObjects) return 0;
	objects[numObjects++] = obj;
	return 1;
}

void CollisionNode::clear() {
	numObjects = 0;
}

CollisionGrid::CollisionGrid(uint16_t m, uint16_t n, PhysicsController* ctrlr) : GridContainer<CollisionNode>(m, n) {
	controlledBy = ctrlr;
}


void CollisionGrid::checkCollision(PhysicsObject* obj1, PhysicsObject* obj2) {
	glm::vec2 distanceVector = obj1->position - obj2->position;
	float dist = glm::length(distanceVector);
	float minDist = obj1->radius + obj2->radius;
	if (dist < minDist) {
		glm::vec2 collisionAxis = distanceVector / dist;
		glm::vec2 tangentAxis = glm::vec2(-collisionAxis.y, collisionAxis.x);

		float delta = minDist - dist;
		float repositionAmount = .5f * delta;
		obj1->position += repositionAmount * glm::normalize(collisionAxis + obj1->normalForce);
		obj2->position -= repositionAmount * glm::normalize(collisionAxis + obj2->normalForce);

		obj1->addNormalDirection(obj2->normalForce);
		obj2->addNormalDirection(obj1->normalForce);

		float factorMass1 = 2 * obj1->mass / (obj1->mass + obj2->mass);
		float factorMass2 = 2 * obj2->mass / (obj1->mass + obj2->mass);
		glm::vec2 positionDiffVector = obj1->position - obj2->position;
		glm::vec2 velocityDiffVector = obj1->velocity - obj2->velocity;

		glm::vec2 velocityAdjustment1 = factorMass1 * glm::dot(velocityDiffVector, positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * positionDiffVector;
		glm::vec2 velocityAdjustment2 = factorMass2 * glm::dot(-velocityDiffVector, -positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * -positionDiffVector;
		obj1->velocity -= velocityAdjustment1 * ELASTICITY;
		obj2->velocity -= velocityAdjustment2 * ELASTICITY;

		obj1->enforceBoundaries(controlledBy->simulationWidth, controlledBy->simulationHeight);
		obj2->enforceBoundaries(controlledBy->simulationWidth, controlledBy->simulationHeight);
	}
}

void CollisionGrid::checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2) {
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
	 
void CollisionGrid::handleCollisions(int widthLow = 1, int widthHigh = -1) {
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

void CollisionGrid::handleCollisionsThreaded() {
	std::thread threads[THREAD_COUNT] = {};
	float step = static_cast<float>(width) / static_cast<float>(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; i++) {
		int widthRangeLow = static_cast<int>(step * i) + 1;
		int widthRangeHigh = static_cast<int>(step * (i + 1)) + 1;
		threads[i] = std::thread(&CollisionGrid::handleCollisions, this, widthRangeLow, widthRangeHigh);
	} 

	for (int i = 0; i < THREAD_COUNT; i++) {
		threads[i].join();
	}
}



template <typename T>
ObjectSpawner<T>::ObjectSpawner(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag) {
	controller = ctrlr;
	position = p;
	exitVelocity = mag * dir;
}

template <typename T>
void ObjectSpawner<T>::shoot(float timeDelta) {
	controller->addObject(new T(position, OBJECT_SIZE, exitVelocity));
}

template <typename T>
void ObjectSpawner<T>::update(float timeDelta) {
	if (!keepShooting) return;
	timeSinceLastShot += timeDelta;
	if (timeSinceLastShot > REFRACTORY_TIME) {
		shoot(timeDelta);
		timeSinceLastShot -= REFRACTORY_TIME;
	}
}

template <typename T>
void ObjectSpawner<T>::start() {
	keepShooting = true;
}

template <typename T>
void ObjectSpawner<T>::stop() {
	keepShooting = false;
}


PhysicsController::PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_) {
	simulationWidth = simulationWidth_;
	simulationHeight = simulationHeight_;

	uint16_t gridWidth = floor(static_cast<float>(simulationWidth) / CELL_SIZE) + 3;
	uint16_t gridHeight = floor(static_cast<float>(simulationHeight) / CELL_SIZE) + 3;

	grid = new CollisionGrid(gridWidth, gridHeight, this);

	addSpawnerN({ 75, 75 }, { 1, 0 }, SPAWNER_EXIT_SPEED, 5);
}

PhysicsController::~PhysicsController() {
	for (PhysicsObject* obj : objects) delete obj;
	for (auto spawner : spawners) delete spawner;
	delete grid;
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
	for (PhysicsObject* obj : objects) {
		obj->accelerate(glm::vec2(0, GRAVITAIONAL_FORCE));
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
		glm::u8vec2 gridIndex = obj->position / static_cast<float>(CELL_SIZE) + glm::vec2(1,1);
		assert(grid->insert(obj, gridIndex));
	}
#ifdef USE_THREAD
	grid->handleCollisionsThreaded();
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