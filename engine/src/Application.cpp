#include "Application.hpp"

#include <GLFW/glfw3.h>
// #define GLFW_EXPOSE_NATIVE_WIN32
// #include <GLFW/glfw3native.h>

#include <spdlog/spdlog.h>
// #include <imgui.h>
// #include <backends/imgui_impl_dx11.h>
// #include <backends/imgui_impl_win32.h>

#pragma region glfw callbacks

static void WindowSizeCallback(GLFWwindow* window, int width, int height) {
	void* user = glfwGetWindowUserPointer(window);
	assert(user != nullptr);
	Application* application = (Application*)user;

	application->OnWindowResize(window);
}

#pragma endregion

Application::Application()
{
	// ImGui::
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

	// IMGUI_CHECKVERSION();
	// ImGui::CreateContext();
	// ImGuiIO& io = ImGui::GetIO();
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
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

	m_renderer = std::make_unique<Renderer>(m_window);

	// ImGui_ImplWin32_Init(glfwGetWin32Window(m_window));
	// ImGui_ImplDX11_Init(m_renderer->GetDevice().Get(), m_renderer->GetDeviceContext().Get());

	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();

		m_renderer->Render();
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
	m_renderer->HandleResize(width, height);
}
