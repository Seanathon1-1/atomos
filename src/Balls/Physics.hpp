#pragma once
#include <vector>
#include <glm.hpp>
#include <thread>
#include <array>
#include <queue>
#include <mutex>
#include "GridContainer.hpp"
#include <future>

#define REFRACTORY_TIME .115f
#define MAX_COLLISION_NODE_OBJECTS 4

// forward declarations
class PhysicsController;
class ThreadPool;


class ThreadPool {
	std::vector<std::thread> threads;
	std::queue<std::function<void()>> taskQueue;

	mutable std::mutex mutex;
	std::condition_variable cv;
	
	bool shutdownRequested = false;
	int workingThreads;

	struct WorkerThread {
		ThreadPool* pool;

		WorkerThread(ThreadPool* pool_) : pool(pool_) {}
		void operator()() {
			// take mutex
			std::unique_lock<std::mutex> lock(pool->mutex);

			while (!pool->shutdownRequested || !pool->taskQueue.empty()) {
				// wait on the condition variable until woken up
				pool->workingThreads--;
				pool->cv.wait(lock, [this] {
					return this->pool->shutdownRequested || !this->pool->taskQueue.empty();
					});
				pool->workingThreads++;

				// take a task and perform the function
				if (!this->pool->taskQueue.empty()) {
					auto func = pool->taskQueue.front();
					pool->taskQueue.pop();

					lock.unlock();
					func();
					lock.lock();
				}
			}
		}
	};

public:
	// make the threads
	ThreadPool(const uint8_t numThreads) : workingThreads(numThreads) {
		for (int i{ numThreads }; i--;) {
			threads.push_back(std::thread(WorkerThread(this)));
		}
	}

	~ThreadPool() {
		// wake all threads to join them all
		{
			std::lock_guard<std::mutex> lock(mutex);
			shutdownRequested = true;
			cv.notify_all();
		}

		// join the threads
		for (int i = 0; i < threads.size(); i++) {
			if (threads[i].joinable()) {
				threads[i].join();
			}
		}
	}

	// wrap a function and its arguments 
	template <typename F, typename... Args>
	auto addTask(F&& function, Args&&... arguments) -> std::future<decltype(function(arguments...))> {
		// bind function and arguments together before packing
		auto boundFunction = std::bind(std::forward<F>(function), std::forward<Args>(arguments)...);

		// build a packaged_task as a shared resource for the threads
		auto sharedTask = std::make_shared<std::packaged_task<decltype(function(arguments...))()>>(boundFunction);

		// lambda wrapper function to place the task in the queue
		auto wrapperLambda = [sharedTask]() { (*sharedTask)(); };

		// grab the mutex before queue operations		
		// scoped to force a call to the lock's destructor
		{
			std::lock_guard<std::mutex> lock(mutex);
			taskQueue.push(wrapperLambda);
			cv.notify_one();
		}

		return sharedTask->get_future();
	}
};





struct PhysicsObject {
	glm::vec2 position;
	glm::vec2 position_old;
	glm::vec2 acceleration;
	float radius;
	uint32_t color;
	float mass;

	PhysicsObject(glm::vec2 pos, float r, glm::vec2 v);
	void accelerate(glm::vec2 acc);
	void enforceBoundaries(uint16_t width, uint16_t height);
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
	PhysicsController* controlledBy;

public:
	CollisionGrid(uint16_t m, uint16_t n, PhysicsController* ctrlr);
	void checkCollision(PhysicsObject* obj1, PhysicsObject* obj2);
	void checkCellCollisions(CollisionNode* cell1, CollisionNode* cell2);
	void handleCollisions(int widthLow, int widthHigh);
	void handleCollisionsThreaded(ThreadPool* pool);
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
	CollisionGrid* grid;
	ThreadPool* pool;

	void handleCollisionsIterations(uint8_t iterations);
	void handleCollisions();
	void addSpawner(PhysicsController* ctrlr, glm::vec2 position, glm::vec2 direction, float magnitude);
	void addSpawnerN(PhysicsController* ctrlr, glm::vec2 p, glm::vec2 dir, float mag, uint8_t n);

protected:
	uint16_t simulationWidth;
	uint16_t simulationHeight;


public:
	PhysicsController(uint16_t simulationWidth_, uint16_t simulationHeight_);
	~PhysicsController();
	size_t getNumObjects();
	void addObject(PhysicsObject* obj);
	void stopSpawners();
	void startSpawners();
	void update(float dt);
	void displaySimulation();

	friend class CollisionGrid;
};