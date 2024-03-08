#pragma once
#include <vector>
#include <glm.hpp>
#include <array>
#include "ThreadPool.hpp"
#include "GridContainer.hpp"

constexpr float REFRACTORY_TIME = .115f;
constexpr int MAX_COLLISION_NODE_OBJECTS = 16;

#define USE_QUEUE

class PhysicsController;

struct PhysicsComponent {
	PhysicsController* controller = 0;
};

class PhysicsController {
	enum Direction {NONE = -1, UP, RIGHT, DOWN, LEFT };

	struct PhysicsObject : PhysicsComponent {
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec2 acceleration;

		PhysicsObject* previous = 0;
		PhysicsObject* next = 0;

		float infrastepTime;
		float radius;
		uint32_t color;
		float mass;

		PhysicsObject(PhysicsController* ctrlr,  glm::vec2 pos, float r, glm::vec2 v);
		~PhysicsObject();
		void accelerate(glm::vec2 acc);
		void enforceBoundaries(uint16_t width, uint16_t height);
		void update(float timeDelta, uint16_t simWidth, uint16_t simHeight);
	};




	struct CollisionNode {
#ifndef USE_QUEUE
		static constexpr uint8_t maxObjects = MAX_COLLISION_NODE_OBJECTS;
		PhysicsObject* objects[maxObjects];
		uint8_t numObjects;
#else
		glm::vec2 minimumBound;
		glm::vec2 maximumBound;
		glm::u16vec2 index;


		PhysicsObject* head = 0;
		PhysicsObject* tail = 0;
		uint8_t numObjects = 0;
#endif

		size_t count() const;
		bool insert(PhysicsObject* obj);
		bool remove(PhysicsObject* obj);
		void clear();
	};


	struct CollisionEvent {
		enum CollisionType {CELL_CHANGE, BOUNDARY_ENFORCEMENT, BALL_BALL};
		
		CollisionType type;
		float eventTime;
		PhysicsObject* subjectObject;
		Direction eventDirection;
		bool isDirty = false;
		
		bool operator<(CollisionEvent otherEvent) {
			return this->eventTime < otherEvent.eventTime;
		}
		bool operator<(const CollisionEvent otherEvent) const {
			return this->eventTime < otherEvent.eventTime;
		}
	};
	typedef std::priority_queue<CollisionEvent> CollisionQueue;


	class CollisionGrid : public GridContainer<CollisionNode> {
		CollisionQueue eventQueue;
		
	public:
		CollisionGrid(uint16_t m, uint16_t n, PhysicsController* ctrlr);
		void checkCollision(PhysicsObject* obj1, PhysicsObject* obj2);
		void checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2);
		void handleCollisions(int widthLow, int widthHigh);
		void handleCollisionsThreaded(ThreadPool* pool);
		void addCollisionsToQueue(PhysicsObject* object, float dt);
		void checkCollisionsQueue();
	};






	template <typename T>
	class ObjectSpawner : PhysicsComponent {
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
	// End inner classes





	// Physics Controller Members
	std::vector<PhysicsObject*> objects;	
	std::vector<ObjectSpawner<PhysicsObject>*> spawners;
	CollisionGrid* grid;
	ThreadPool* pool;

	void handleCollisionsIterations(uint8_t iterations);
	void handleCollisions();
	void addSpawner(glm::vec2 position, glm::vec2 direction, float magnitude);
	void addSpawnerN(glm::vec2 p, glm::vec2 dir, float mag, uint8_t n);

protected:
	uint16_t simulationWidth;
	uint16_t simulationHeight;
	void addObject(PhysicsObject* obj);
	
	template <typename T>
	friend class ObjectSpawner;


public:
	PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_);
	~PhysicsController();
	size_t getNumObjects();
	void stopSpawners();
	void startSpawners();
	void update(float dt);
	void displaySimulation();

	friend class CollisionGrid;
};