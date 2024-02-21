#include <chrono>

class Timer {
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point splitTime;
	std::chrono::steady_clock::time_point stopTime;

	bool running = false;
	
public:
	Timer();
	~Timer();
	void start();
	void stop();
	float readSplitMillis();
	float readmarkSplitMillis();
	float readTime();
	void markSplit();
};