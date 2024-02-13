#include "Physics.hpp"
#include "imgui.h"

#define ELASTICITY .5f
#define IMGUI_FRAME_MARGIN 4
#define GRAVITAIONAL_FORCE 50
#define SPAWNER_EXIT_SPEED 150
#define SPAWNER_OFFSET glm::vec2(0, 10)
#define OBJECT_SIZE 4
#define DENSITY 2
#define COLLISION_ITERATIONS 12

class PhysicsController;

PhysicsObject::PhysicsObject(glm::vec2 pos, float r, glm::vec2 v, uint32_t col = 0xffff00ff) {
	position = pos;
	velocity = v;
	acceleration = glm::vec2(0);
	radius = r;
	color = col;
	mass = r * DENSITY;
}

void PhysicsObject::accelerate(glm::vec2 acc) {
	acceleration += acc;
}

void PhysicsObject::update(float timeDelta, uint16_t simWidth, uint16_t simHeight) {
	velocity += acceleration * timeDelta;
	position += velocity * timeDelta;
	acceleration = glm::vec2(0);

	if (position.y > simHeight - radius - IMGUI_FRAME_MARGIN) {
		position.y = simHeight - radius - IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
	}
	if (position.y < radius + IMGUI_FRAME_MARGIN) {
		position.y = radius + IMGUI_FRAME_MARGIN;
		velocity.y *= -ELASTICITY;
	}

	if (position.x > simWidth - radius - IMGUI_FRAME_MARGIN) {
		position.x = simWidth - radius - IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
	}
	if (position.x < radius + IMGUI_FRAME_MARGIN) {
		position.x = radius + IMGUI_FRAME_MARGIN;
		velocity.x *= -ELASTICITY;
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
				glm::vec2 distanceVector = obj1->position - obj2->position;
				float dist2 = glm::dot(distanceVector, distanceVector);
				float minDist = obj1->radius + obj2->radius;
				if (dist2 < minDist * minDist) {
					float dist = sqrtf(dist2);
					glm::vec2 collisionAxis = distanceVector / dist;
					glm::vec2 tangentAxis = glm::vec2(-collisionAxis.y, collisionAxis.x);
					
					float delta = minDist - dist;
					obj1->position += .5f * delta * collisionAxis;
					obj2->position -= .5f * delta * collisionAxis;

					float factorMass1 = 2 * obj1->mass / (obj1->mass + obj2->mass);
					float factorMass2 = 2 * obj2->mass / (obj1->mass + obj2->mass);
					glm::vec2 positionDiffVector = obj1->position - obj2->position;
					glm::vec2 velocityDiffVector = obj1->velocity - obj2->velocity;
					
					obj1->velocity -= factorMass1 * glm::dot(velocityDiffVector, positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * positionDiffVector;
					obj2->velocity -= factorMass2 * glm::dot(-velocityDiffVector, -positionDiffVector) / glm::dot(positionDiffVector, positionDiffVector) * -positionDiffVector;
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

	addSpawnerN(this, { 75, 75 }, { 1, 0 }, SPAWNER_EXIT_SPEED, 5);
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
	if (frames < 50) { stopSpawners(); }
	if (frames > 80) { startSpawners(); }
	
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
	grid->clear();
	for (PhysicsObject* obj : objects) {
		glm::vec2 gridIndex = obj->position / (float)OBJECT_SIZE;
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
		ImVec2 posBallOffset = { obj->position.x + posWindowOffset.x, obj->position.y + posWindowOffset.y };
		ImGui::GetWindowDrawList()->AddCircleFilled(posBallOffset, obj->radius, obj->color);
	}

	ImGui::End();
}