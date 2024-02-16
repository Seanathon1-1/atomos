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
#include <glm.hpp>
#include "Physics.hpp"

#include <chrono>
#include <vector>
#include <iostream>


#define SIMULATION_WINDOW_WIDTH 800
#define SIMULATION_WINDOW_HEIGHT 700


void framebuffer_size_callback(GLFWwindow* window, int width, int height);	
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

bool debug;

// timing
std::chrono::steady_clock::time_point timeCurrent;
std::chrono::steady_clock::time_point timeLastFrame;
std::chrono::duration<double, std::milli> delta;
float frames = 0.f;
float deltaTime = 0.f;

bool useCustomDT = false;
float customDeltaTime = 0.006f;

PhysicsController* physics; 

void showDebugWindow() {
	ImGui::Begin("debug");
	ImGui::Text("Frames: %d", (int)frames);
	ImGui::Text("Objects: %d", physics->getNumObjects());
	ImGui::Text("Time Delta: %f", deltaTime);
	//ImGui::Checkbox("Custom deltaTime", &useCustomDT);
	//ImGui::DragFloat("Custom deltaTime value", &customDeltaTime, 0.00001f, 0.0005, 0.013, "%.4f");
	ImGui::End();
}

int main()
{
#ifdef DEBUG
	debug = 1;
#endif
	physics = new PhysicsController(SIMULATION_WINDOW_WIDTH, SIMULATION_WINDOW_HEIGHT);

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

		physics->displaySimulation();
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
		deltaTime = static_cast<float>(delta.count()) / 1000;
		frames = 1 / deltaTime;
		if (useCustomDT) deltaTime = customDeltaTime;

		timeLastFrame = timeCurrent;

		physics->update(deltaTime);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
	}

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
