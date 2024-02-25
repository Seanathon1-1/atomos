#include "Physics.hpp"
#include "imgui.h"
#include <iostream>
#include <thread>

#define ELASTICITY .6f
#define IMGUI_FRAME_MARGIN 4
#define GRAVITAIONAL_FORCE 45
#define SPAWNER_EXIT_SPEED 2.4f
#define MAX_SPEED SPAWNER_EXIT_SPEED * 3.5f
#define OBJECT_SIZE 2 
#define CELL_SIZE (OBJECT_SIZE * 2)
#define MAX_OBJECTS 4300
#define SPAWNER_OFFSET glm::vec2(-OBJECT_SIZE * 2, OBJECT_SIZE * 2 + 2)
#define DENSITY 2
#define COLLISION_ITERATIONS 5
#define THREAD_COUNT 4
#define EPSILON 0.01
#define MAX_TIME_STEP (1.f / 60.f)


#define USE_COLLISION_GRID
#define USE_THREADS

class PhysicsController;


static uint16_t objCount = 0;
 
PhysicsObject::PhysicsObject(glm::vec2 pos, float r, glm::vec2 v) {
	position = pos;
	position_old = pos - v;
	acceleration = glm::vec2(0);
	radius = r;	
	mass = r * r * DENSITY;
	
	uint16_t hue = objCount % 360;
	float fun = 1 - abs( fmod(static_cast<float>(hue) / 60.f, 2) - 1);
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

void PhysicsObject::accelerate(glm::vec2 acc) {
	acceleration += acc;
}

void PhysicsObject::enforceBoundaries(uint16_t width, uint16_t height) {
	glm::vec2 velocity = position_old - position;
	if (position.y > height - radius - IMGUI_FRAME_MARGIN) {
		velocity *= glm::vec2(1, -1);
		position.y = height - radius - IMGUI_FRAME_MARGIN;
		position_old = position + velocity * ELASTICITY;
	}
	if (position.y < radius + IMGUI_FRAME_MARGIN) {
		velocity *= glm::vec2(1, -1);
		position.y = radius + IMGUI_FRAME_MARGIN;
		position_old = position + velocity * ELASTICITY;
	}

	if (position.x > width - radius - IMGUI_FRAME_MARGIN) {
		velocity *= glm::vec2(-1, 1);
		position.x = width - radius - IMGUI_FRAME_MARGIN;
		position_old = position + velocity * ELASTICITY;
	}
	if (position.x < radius + IMGUI_FRAME_MARGIN) {
		velocity *= glm::vec2(-1, 1);
		position.x = radius + IMGUI_FRAME_MARGIN;
		position_old = position + velocity * ELASTICITY;
	}
}

void PhysicsObject::update(float timeDelta, uint16_t simWidth, uint16_t simHeight) {
	glm::vec2 velocity = position - position_old;
	float speed = glm::length(velocity);
	color = (speed > MAX_SPEED) ? 0xFF0000FF : 0xFFFFFFFF;
	position_old = position;
	position += velocity + acceleration * timeDelta * timeDelta;
	acceleration = glm::vec2(0);
	if (speed < EPSILON) position_old = position;
	enforceBoundaries(simWidth, simHeight);
}

size_t CollisionNode::count() const {
	return numObjects;
}

bool CollisionNode::insert(PhysicsObject* obj) {
	assert(numObjects < maxObjects);
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
		obj1->position += .5f * delta * collisionAxis;
		obj2->position -= .5f * delta * collisionAxis;

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
	if (widthHigh == -1 || widthHigh >= width) widthHigh = width - 1;

	for (int j = 1; j < height - 1; j++) {
		for (int i = 1; i < width - 1; i++) {
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

void CollisionGrid::handleCollisionsThreaded(ThreadPool* pool) {
	float step = static_cast<float>(width) / static_cast<float>(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; i++) {
		int widthRangeLow = static_cast<int>(step * i) + 1;
		int widthRangeHigh = static_cast<int>(step * (i + 1)) + 1;
		pool->addTask(std::bind(&CollisionGrid::handleCollisions, this, widthRangeLow, widthRangeHigh));
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
	pool = new ThreadPool(THREAD_COUNT);

	addSpawnerN(this, { 75, 75 }, { 1, 0 }, SPAWNER_EXIT_SPEED, 5);
}

PhysicsController::~PhysicsController() {
	for (PhysicsObject* obj : objects) delete obj;
	for (auto spawner : spawners) delete spawner;
	delete grid;
	delete pool;
}

void PhysicsController::addSpawner(PhysicsController* ctrlr, glm::vec2 position, glm::vec2 direction, float magnitude) {
	spawners.emplace_back(new ObjectSpawner<PhysicsObject>(ctrlr, position + SPAWNER_OFFSET * static_cast<float>(spawners.size()), direction, magnitude));
}

void PhysicsController::addSpawnerN(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag, uint8_t n) { 
	for (uint8_t i{ n }; i--;) addSpawner(ctrlr, p, dir, mag); 
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