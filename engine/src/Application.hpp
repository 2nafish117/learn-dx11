#pragma once

#include <memory>

#include "Renderer.hpp"

struct GLFWwindow;

class Application {

public:
	Application();
	virtual ~Application();
	int Run();

private:
	void OnWindowResize(GLFWwindow* window);

private:
	GLFWwindow* m_window = nullptr;
	std::unique_ptr<Renderer> m_renderer;

	// glfw callbacks
	friend void WindowSizeCallback(GLFWwindow* window, int width, int height);
};