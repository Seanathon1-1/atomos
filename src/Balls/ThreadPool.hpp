#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <functional>


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
