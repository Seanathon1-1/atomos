#include "Physics.hpp"
#include "imgui.h"

#define ELASTICITY .1f
#define IMGUI_FRAME_MARGIN 4
#define GRAVITAIONAL_FORCE 50
#define SPAWNER_EXIT_SPEED 120
#define SPAWNER_OFFSET glm::vec2(0, 10)
#define OBJECT_SIZE 4

class PhysicsController;

PhysicsObject::PhysicsObject(glm::vec2 pos, float r, glm::vec2 v, uint32_t col = 0xffff00ff) {
	positionCurrent = pos;
	positionOld = { pos.x - v.x, pos.y - v.y };
	acceleration = glm::vec2(0);
	radius = r;
	color = col;
}

void PhysicsObject::accelerate(glm::vec2 acc) {
	acceleration += acc;
}

void PhysicsObject::update(float timeDelta, uint16_t simWidth, uint16_t simHeight) {
	glm::vec2 velocity = positionCurrent - positionOld;
	glm::vec2 newPosition = positionCurrent + velocity + acceleration * timeDelta * timeDelta;
	positionOld = positionCurrent;
	positionCurrent = newPosition;
	acceleration = glm::vec2(0);

	float correctedPosition;
	if (positionCurrent.y > simHeight - radius - IMGUI_FRAME_MARGIN) {
		correctedPosition = simHeight - radius - IMGUI_FRAME_MARGIN;
		positionOld.y = (positionCurrent.y - positionOld.y) * ELASTICITY + correctedPosition;
		positionCurrent.y = correctedPosition;
	}
	if (positionCurrent.y < radius + IMGUI_FRAME_MARGIN) {
		correctedPosition = radius + IMGUI_FRAME_MARGIN;
		positionOld.y = (positionCurrent.y - positionOld.y) * ELASTICITY + correctedPosition;
		positionCurrent.y = correctedPosition;
	}
	if (positionCurrent.x > simWidth - radius - IMGUI_FRAME_MARGIN) {
		correctedPosition = simWidth - radius - IMGUI_FRAME_MARGIN;
		positionOld.x = (positionCurrent.x - positionOld.x) * ELASTICITY + correctedPosition;
		positionCurrent.x = correctedPosition;
	}
	if (positionCurrent.x < radius + IMGUI_FRAME_MARGIN) {
		correctedPosition = radius + IMGUI_FRAME_MARGIN;
		positionOld.x = (positionCurrent.x - positionOld.x) * ELASTICITY + correctedPosition;
		positionCurrent.x = correctedPosition;
	}
}
typedef std::vector<PhysicsObject*> PhysObjs;


size_t CollisionNode::count() {
	return numObjects;
}

bool CollisionNode::insert(PhysicsObject* obj) {
	if (numObjects >= maxObjects) return 0;
	objects[numObjects++] = obj;
	return 1;
}

void CollisionNode::clear() {
	numObjects = 0;
}

CollisionGrid::CollisionGrid(uint16_t m, uint16_t n) : GridContainer<CollisionNode>(m, n) {}


void CollisionGrid::checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2) {
	for (int i = 0; i < cell1->numObjects; i++) {
		PhysicsObject* obj1 = cell1->objects[i];
		for (int j = 0; j < cell2->numObjects; j++) {
			PhysicsObject* obj2 = cell2->objects[j];
			if (obj1 != obj2) {
				glm::vec2 collisionAxis = obj1->positionCurrent - obj2->positionCurrent;
				float dist = glm::length(collisionAxis);
				float minDist = obj1->radius + obj2->radius;
				if (dist < minDist) {
					glm::vec2 normalizedAxis = collisionAxis / dist;
					float delta = minDist - dist;
					obj1->positionCurrent += .5f * delta * normalizedAxis;
					obj2->positionCurrent -= .5f * delta * normalizedAxis;
				}
			}
		}
	}
}
	 
void CollisionGrid::handleCollisions() {
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


template <typename T>
ObjectSpawner<T>::ObjectSpawner(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag) {
	controller = ctrlr;
	position = p;
	exitVelocity = mag * dir;
}

template <typename T>
void ObjectSpawner<T>::shoot(float timeDelta) {
	controller->addObject(new T(position, OBJECT_SIZE, exitVelocity * timeDelta));
}

template <typename T>
void ObjectSpawner<T>::update(float timeDelta) {
	timeSinceLastShot += timeDelta;
	if (!keepShooting) return;
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

void PhysicsController::addSpawner(PhysicsController* ctrlr, glm::vec2 position, glm::vec2 direction, float magnitude) {
	spawners.emplace_back(new ObjectSpawner<PhysicsObject>(ctrlr, position + SPAWNER_OFFSET * static_cast<float>(spawners.size()), direction, magnitude));
}

void PhysicsController::addSpawnerN(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag, uint8_t n) { 
	for (uint8_t i{ n }; i--;) addSpawner(ctrlr, p, dir, mag); 
}

PhysicsController::PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_) {
	simulationWidth = simulationWidth_;
	simulationHeight = simulationHeight_;

	grid = new CollisionGrid(simulationWidth / OBJECT_SIZE, simulationHeight / OBJECT_SIZE);

	addSpawnerN(this, { 75, 75 }, { 1, 0 }, SPAWNER_EXIT_SPEED, 2);
}

PhysicsController::~PhysicsController() {
	for (PhysicsObject* obj : objects) delete obj;
	for (auto spawner : spawners) delete spawner;
	delete grid;
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

void PhysicsController::update(float dt, float frames) {
	if (frames < 60) { stopSpawners(); }
	if (frames > 75) { startSpawners(); }
	
	grid->clear();
	for (auto spawner : spawners) {
		spawner->update(dt);
	}
	for (PhysicsObject* obj : objects) {
		obj->accelerate(glm::vec2(0, GRAVITAIONAL_FORCE));
		obj->update(dt, simulationWidth, simulationHeight);
	}
	handleCollisions();
}

void PhysicsController::handleCollisions() {
	for (PhysicsObject* obj : objects) {
		glm::vec2 gridIndex = obj->positionCurrent / (float)OBJECT_SIZE;
		//std::cerr << "Adding ball to index (" << gridIndex.x << ", " << gridIndex.y << ")\n";
		grid->insert(obj, gridIndex);
	}
	grid->handleCollisions();
}

void PhysicsController::displaySimulation() {
	ImGui::SetNextWindowSize({ static_cast<float>(simulationWidth) + IMGUI_FRAME_MARGIN, static_cast<float>(simulationHeight) + IMGUI_FRAME_MARGIN });
	ImGui::SetNextWindowContentSize({ static_cast<float>(simulationWidth), static_cast<float>(simulationHeight) });
	ImGui::Begin("balls", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	for (PhysicsObject* obj : objects) {
		ImVec2 posWindowOffset = ImGui::GetWindowPos();
		ImVec2 posBallOffset = { obj->positionCurrent.x + posWindowOffset.x, obj->positionCurrent.y + posWindowOffset.y };
		ImGui::GetWindowDrawList()->AddCircleFilled(posBallOffset, obj->radius, obj->color);
	}

	ImGui::End();
}