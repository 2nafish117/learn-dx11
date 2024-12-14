#include "Application.hpp"

#include <spdlog/spdlog.h>
#include <flags.h>

#include "DX11/DX11Context.hpp"
#include "AssetSystem.hpp"
#include "SceneSystem.hpp"

#include <GLFW/glfw3.h>

namespace global
{
	DX11Context* rendererSystem = nullptr;
}

#pragma region glfw callbacks

void WindowSizeCallback(GLFWwindow* window, int width, int height) {
	void* user = glfwGetWindowUserPointer(window);
	ENSURE(user != nullptr, "");
	Application* application = (Application*)user;

	application->OnWindowResize(window);
}

#pragma endregion

Application::Application(int argc, char** argv)
{
	// init logging
	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%L%$] [thread %t] %v");
	spdlog::info("Logging initialised");

	const flags::args args(argc, argv);

	// init global systems
	{
		global::assetSystem = new AssetSystem();
		{
			ASSERT(global::assetSystem != nullptr, "");

			const auto dataDir = args.get<std::string_view>("data_dir", "data");
			spdlog::info("using data directory: {}", dataDir);
			global::assetSystem->SetDataDir(dataDir);
		}

		global::sceneSystem = new SceneSystem();
		{
			ASSERT(global::sceneSystem != nullptr, "");
			global::sceneSystem->runtimeScene = std::make_shared<RuntimeScene>();
		}
	}
}

Application::~Application()
{
	// deinit global systems
	{
		delete global::rendererSystem;
		delete global::assetSystem;
		delete global::sceneSystem;
	}
}

int Application::Run()
{
	spdlog::info("Application startup");
	if(int res = glfwInit(); !res) 
	{
		spdlog::critical("glfwInit failed with {}", res);
		return -1;
	}

	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(1280, 720, "Engine", nullptr, nullptr);
	
	glfwSetWindowSizeCallback(m_window, WindowSizeCallback);
	glfwSetWindowUserPointer(m_window, this);

	if (!m_window)
	{
		spdlog::critical("window creation failed");
		glfwTerminate();
		return -1;
	}

	global::rendererSystem = new DX11Context(m_window);

	global::assetSystem->RegisterAssets();

	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		
		global::rendererSystem->Render(*global::sceneSystem->runtimeScene.get());
	}

	glfwDestroyWindow(m_window);
	spdlog::info("window destroyed");

	glfwTerminate();

	spdlog::info("Application closing");
	return 0;
}

void Application::OnWindowResize(GLFWwindow* window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	global::rendererSystem->HandleResize(width, height);
}
