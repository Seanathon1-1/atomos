#include "Application.hpp"

#include <vector>
#include <iostream>

#define SIMULATION_WINDOW_WIDTH 800
#define SIMULATION_WINDOW_HEIGHT 700

#define TIME_STEP 1.f / 60.f

class BallsApp : protected Application {  };

int main() {
    BallsApp* app = new BallsApp();
	return 0;
}