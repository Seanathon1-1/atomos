#pragma once
#include <vector>
#include <glm.hpp>
#include "GridContainer.hpp"

#define REFRACTORY_TIME .085f
#define MAX_COLLISION_NODE_OBJECTS 16

class PhysicsController;

struct PhysicsObject {
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec2 acceleration;
	float radius;
	uint32_t color;
	float mass;

	PhysicsObject(glm::vec2 pos, float r, glm::vec2 v, uint32_t col);
	void accelerate(glm::vec2 acc);
	void update(float timeDelta, uint16_t simWidth, uint16_t simHeight);
};
typedef std::vector<PhysicsObject*> PhysObjs;


struct CollisionNode {
	static constexpr uint8_t maxObjects = MAX_COLLISION_NODE_OBJECTS;
	PhysicsObject* objects[maxObjects];
	uint8_t numObjects;

	size_t count() const;
	bool insert(PhysicsObject* obj);
	void clear();
};

class CollisionGrid : public GridContainer<CollisionNode> {
	using GridContainer<CollisionNode>::width;

public:
	CollisionGrid(uint16_t m, uint16_t n);
	void checkCollision(PhysicsObject* obj1, PhysicsObject* obj2);
	void checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2);
	void handleCollisions();
};

template <typename T>
class ObjectSpawner {
	PhysicsController* controller;
	glm::vec2 position;
	glm::vec2 exitVelocity;

	float timeSinceLastShot = REFRACTORY_TIME;
	bool keepShooting = true;

	void shoot(float timeDelta);
public:
	ObjectSpawner(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag);
	void update(float timeDelta);
	void start();
	void stop();
};


class PhysicsController {
	std::vector<PhysicsObject*> objects;
	std::vector<ObjectSpawner<PhysicsObject>*> spawners;
	uint16_t simulationWidth;
	uint16_t simulationHeight;
	CollisionGrid* grid;

	void handleCollisionsIterations(uint8_t iterations);
	void handleCollisions();
	void addSpawner(PhysicsController* ctrlr, glm::vec2 position, glm::vec2 direction, float magnitude);
	void addSpawnerN(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag, uint8_t n);
public:
	PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_);
	~PhysicsController();
	size_t getNumObjects();
	void addObject(PhysicsObject* obj);
	void stopSpawners();
	void startSpawners();
	void update(float dt);
	void displaySimulation();
};