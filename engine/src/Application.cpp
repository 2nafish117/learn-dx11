#include "Application.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

Application::Application()
{
	// https://github.com/gabime/spdlog/wiki/3.-Custom-formatting#pattern-flags
	spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%L%$] [thread %t] %v");
	spdlog::info("Logging initialised");

	// spdlog::trace("log trace");
	// spdlog::info("log info");
	// spdlog::debug("log debug");
	// spdlog::warn("log warn");
	// spdlog::error("log error");
	// spdlog::critical("log critical");
}

Application::~Application()
{
}

int Application::Run()
{
	spdlog::info("Application startup");
    if(int res = glfwInit(); !res) 
	{
		spdlog::critical("glfwInit failed with {}", res);
        return -1;
	}

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwSwapInterval(1);

    m_window = glfwCreateWindow(1280, 720, "Engine", nullptr, nullptr);
	
    if (!m_window)
    {
		spdlog::critical("window creation failed");
        glfwTerminate();
        return -1;
    }

	m_renderer = std::make_unique<Renderer>();

    glfwMakeContextCurrent(m_window);

    while (!glfwWindowShouldClose(m_window))
    {
        glfwSwapBuffers(m_window);

        glfwPollEvents();
    }

	glfwDestroyWindow(m_window);
	spdlog::info("window destroyed");

    glfwTerminate();
    
	spdlog::info("Application closing");
	return 0;
}
