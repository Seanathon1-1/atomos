#include "Timer.hpp"

Timer::Timer() {

}

Timer::~Timer() {

}

void Timer::start() {
	splitTime = startTime = std::chrono::steady_clock::now();
	running = true;
}

void Timer::stop() {
	stopTime = std::chrono::steady_clock::now();
	running = false;
}

float Timer::readSplitMillis() {
	return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - splitTime).count();
}

float Timer::readmarkSplitMillis()
{
	float returnValue = readSplitMillis();
	markSplit();
	return returnValue;
}

float Timer::readTime() {
	std::chrono::duration<float, std::milli> diff;
	if (running) {
		diff = std::chrono::steady_clock::now()- startTime;
	}
	else {
		diff = stopTime - startTime;
	}
	return diff.count();
}

void Timer::markSplit() {
	splitTime = std::chrono::steady_clock::now();
}
