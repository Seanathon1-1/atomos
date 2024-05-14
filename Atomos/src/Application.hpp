struct GLFWwindow;

class Application {
    GLFWwindow* window;

protected:
    void run();

public:
    Application();
    ~Application();
    void init();
    void onDisplay();
    void onUpdate();
};
