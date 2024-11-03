#pragma once

#include "Basic.hpp"

// class DX11Context;
class ShaderCompiler;
class DeferredRenderer;

struct GLFWwindow;


class Application {

public:
	Application(int argc, char** argv);
	virtual ~Application();
	int Run();

private:
	void OnWindowResize(GLFWwindow* window);

private:
	GLFWwindow* m_window = nullptr;

	std::unique_ptr<DeferredRenderer> m_renderer;

	// glfw callbacks
	friend void WindowSizeCallback(GLFWwindow* window, int width, int height);
};