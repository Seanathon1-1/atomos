// Taken from https://learnopengl.com/code_viewer_gh.php?code=src/1.getting_started/1.2.hello_window_clear/hello_window_clear.cpp and modified to work with glfw
// All credit go to Joey De Vries: https://joeydevries.com/#home author of Learn OpenGL.
// Modified by: Jakob Törmä Ruhl
/* Functionality extended by Síofra McCarthy */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL\glew.h>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "glm.hpp"

#include <chrono>
#include <vector>
#include <iostream>

#define GRAVITAIONAL_FORCE 96
#define REFRACTORY_TIME .065f
#define BALL_SIZE 4

#define SIMULATION_WINDOW_WIDTH 800
#define SIMULATION_WINDOW_HEIGHT 700
#define ELASTICITY .6f

#define IMGUI_FRAME_MARGIN 4
#define DELTA_TIME 1.f / 60.f

void framebuffer_size_callback(GLFWwindow* window, int width, int height);	
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

bool debug;

std::chrono::steady_clock::time_point timeCurrent;
std::chrono::steady_clock::time_point timeLastFrame;
std::chrono::duration<double, std::milli> delta;
float frames = 0.f;
float deltaTime = 0.f;

bool useCustomDT = false;
float customDeltaTime = 0.006;

class GridContainer;
GridContainer* grid;
/* My substitute for Verlet Objects */
struct Ball {
	glm::vec2 positionCurrent;
	glm::vec2 positionOld;
	glm::vec2 acceleration;
	float radius;
	uint32_t color;

	Ball(glm::vec2 pos, float r, glm::vec2 v, uint32_t col = 0xffff00ff) {
		positionCurrent = pos;
		positionOld = { pos.x - v.x, pos.y - v.y };
		acceleration = glm::vec2(0);
		radius = r;
		color = col;

	}

	void accelerate(glm::vec2 acc) {
		acceleration += acc;
	}

	void update(float timeDelta) {
		glm::vec2 velocity = positionCurrent - positionOld;
		glm::vec2 newPosition = positionCurrent + velocity + acceleration * timeDelta * timeDelta;
		positionOld = positionCurrent;
		positionCurrent = newPosition;
		acceleration = glm::vec2(0);
		
		float correctedPosition;
		if (positionCurrent.y > SIMULATION_WINDOW_HEIGHT - radius - IMGUI_FRAME_MARGIN) {
			correctedPosition = SIMULATION_WINDOW_HEIGHT - radius - IMGUI_FRAME_MARGIN;
			positionOld.y = (positionCurrent.y - positionOld.y) * ELASTICITY + correctedPosition;
			positionCurrent.y = correctedPosition;
		}
		if (positionCurrent.y < radius + IMGUI_FRAME_MARGIN) {
			correctedPosition = radius + IMGUI_FRAME_MARGIN;
			positionOld.y = (positionCurrent.y - positionOld.y) * ELASTICITY + correctedPosition;
			positionCurrent.y = correctedPosition;
		}
		if (positionCurrent.x > SIMULATION_WINDOW_WIDTH - radius - IMGUI_FRAME_MARGIN) {
			correctedPosition = SIMULATION_WINDOW_WIDTH - radius - IMGUI_FRAME_MARGIN;
			positionOld.x = (positionCurrent.x - positionOld.x) * ELASTICITY + correctedPosition;
			positionCurrent.x = correctedPosition;
		}
		if (positionCurrent.x < radius + IMGUI_FRAME_MARGIN) {
			correctedPosition = radius + IMGUI_FRAME_MARGIN;
			positionOld.x = (positionCurrent.x - positionOld.x) * ELASTICITY + correctedPosition;
			positionCurrent.x = correctedPosition;
		}
	}
};

std::vector<Ball*> balls;
/* Does what you'd expect: shoots Balls */
class BallShooter {
	glm::vec2 position;
	glm::vec2 exitVelocity;

	float timeSinceLastShot = REFRACTORY_TIME;
	bool keepShooting = true;

	void shoot(float timeDelta) {
		balls.push_back(new Ball(position, BALL_SIZE, exitVelocity * timeDelta));
	}
public:
	BallShooter(glm::vec2 p, glm::vec2 dir, float mag) {
		position = p;
		exitVelocity = mag * dir;
	}

	void update(float timeDelta) {
		timeSinceLastShot += timeDelta;
		if (!keepShooting) return;
		if (timeSinceLastShot > REFRACTORY_TIME) {
			shoot(timeDelta);
			timeSinceLastShot -= REFRACTORY_TIME;
		}
	}

	void start() {
		keepShooting = true;
	}

	void stop() {
		keepShooting = false;
	}
};

class GridCell {
	static const uint8_t max_capacity = 8;
	static const uint8_t max_index = max_capacity - 1;
	
	uint32_t objects[max_capacity] = {};
	uint8_t num_objects = 0;
};

class GridContainer {
	static uint32_t nextID;
	uint8_t width;
	uint8_t height;
	std::vector <std::vector <Ball*>*> gridSquares;


	std::vector<Ball*>* getCell(int x, int y) {
		return gridSquares.at(y * width + x);
	}

	std::vector<Ball*>* getCell(glm::u8vec2 pos) {
		return getCell(pos.x, pos.y);
	}
public:
	GridContainer(uint8_t m, uint8_t n) {
		width = m;
		height = n;

		for (int i = 0; i < m * n; i++) {
			gridSquares.push_back(new std::vector<Ball*>());
		}
	}

	~GridContainer() {
		for (auto v : gridSquares) {
			delete v;
		}
	}

	bool insert(Ball* b, glm::u8vec2 position) {
		if (position.x < 0 || position.x >= width || position.y < 0 || position.y >= height) return 0;
		getCell(position)->push_back(b);
		return 1;
	}

	void clear() {
		for (auto v : gridSquares) {
			v->clear();
		}
	}
	
	void checkCellCollisions(std::vector<Ball*>* cell1, std::vector<Ball*>* cell2) {
		for (Ball* ball1 : *cell1) {
			for (Ball* ball2 : *cell2) {
				if (ball1 != ball2) {
					glm::vec2 collisionAxis = ball1->positionCurrent - ball2->positionCurrent;
					float dist = glm::length(collisionAxis);
					float minDist = ball1->radius + ball2->radius;
					if (dist < minDist) {
						glm::vec2 normalizedAxis = collisionAxis / dist;
						float delta = minDist - dist;
						ball1->positionCurrent += .5f * delta * normalizedAxis;
						ball2->positionCurrent -= .5f * delta * normalizedAxis;
					}
				}
			}
		}
	}

	void handleCollisions() {
		for (int j = 1; j < height - 1; j++) {
			for (int i = 1; i < width - 1; i++) {
				std::vector<Ball*>* currentCell = getCell(i, j);
				if (currentCell->size() == 0) continue;
				for (int dj = -1; dj <= 1; dj++) {
					for (int di = -1; di <= 1; di++) {
						std::vector<Ball*>* adjacentCell = getCell(i + di, j + dj);
						if (adjacentCell->size() == 0) continue;
						if (debug) {
							//std::cerr << "Current cell size: " << currentCell->size() << " | Adjacent cell size: " << adjacentCell->size() << std::endl;
						}
						checkCellCollisions(currentCell, adjacentCell);
					}
				}
			}
		}
	}
};

void showBallsWindow() {
	ImGui::SetNextWindowSize({ SIMULATION_WINDOW_WIDTH + IMGUI_FRAME_MARGIN, SIMULATION_WINDOW_HEIGHT + IMGUI_FRAME_MARGIN });
	ImGui::SetNextWindowContentSize({ SIMULATION_WINDOW_WIDTH, SIMULATION_WINDOW_HEIGHT });
	ImGui::Begin("balls", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	for (Ball* ball : balls) {
		ImVec2 posWindowOffset = ImGui::GetWindowPos();
		ImVec2 posBallOffset = { ball->positionCurrent.x + posWindowOffset.x, ball->positionCurrent.y + posWindowOffset.y };
		ImGui::GetWindowDrawList()->AddCircleFilled(posBallOffset, ball->radius, ball->color);
	}

	ImGui::End();
}

void showDebugWindow() {
	ImGui::Begin("debug");
	ImGui::Text("Frames: %d", (int)frames);
	ImGui::Text("Objects: %d", balls.size());
	ImGui::Text("Time Delta: %f", deltaTime);
	//ImGui::Checkbox("Custom deltaTime", &useCustomDT);
	//ImGui::DragFloat("Custom deltaTime value", &customDeltaTime, 0.00001f, 0.0005, 0.013, "%.4f");
	ImGui::End();
}

void handleCollisions() {
	grid->clear();
	
	for (Ball* ball : balls) {
		glm::vec2 gridIndex = ball->positionCurrent / (float)BALL_SIZE;
		//std::cerr << "Adding ball to index (" << gridIndex.x << ", " << gridIndex.y << ")\n";
		grid->insert(ball, gridIndex);
	}

	grid->handleCollisions();
}

int main()
{
#ifdef DEBUG
	debug = 1;
#endif

	BallShooter* shooter = new BallShooter({ 75, 75 }, { 1, 0 }, 280);
	uint8_t gridWidth = SIMULATION_WINDOW_WIDTH / BALL_SIZE;
	uint8_t gridHeight = SIMULATION_WINDOW_HEIGHT / BALL_SIZE;
	grid = new GridContainer(gridWidth, gridHeight);

	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	//init glew after the context have been made
	glewInit();


	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);          // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
	ImGui_ImplOpenGL3_Init();

	// render loop
	// -----------
	timeLastFrame = timeCurrent = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		// input
		// -----
		processInput(window);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		showBallsWindow();
		if (debug) showDebugWindow();

		// render
		// ------
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Rendering
		// (Your code clears your framebuffer, renders your other stuff etc.)
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	
		timeCurrent = std::chrono::steady_clock::now();
		delta = timeCurrent - timeLastFrame;
		deltaTime = delta.count() / 1000;
		frames = 1 / deltaTime;
		if (useCustomDT) deltaTime = customDeltaTime;

		timeLastFrame = timeCurrent;

		if (frames < 60) { shooter->stop(); }
		if (frames > 75) { shooter->start(); }
		shooter->update(DELTA_TIME);

		for (auto ball : balls) {
			ball->accelerate(glm::vec2(0, GRAVITAIONAL_FORCE));
			ball->update(DELTA_TIME);
		}

		handleCollisions();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
	}

	for (Ball* ball : balls) {
		delete ball;
	}
	delete grid;

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}
