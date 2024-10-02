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
	GLFWwindow* m_window;
	std::unique_ptr<Renderer> m_renderer;

};