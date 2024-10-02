#include "Renderer.hpp"

Renderer::Renderer()
{
	if(auto res = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)); FAILED(res)) {
		spdlog::error("CreateDXGIFactory1 err with {}", res);
		return;
	}

	// @TODO: error check
	ComPtr<IDXGIAdapter1> adapter;
	m_factory->EnumAdapters1(0, &adapter);

	// @TODO: error check
	DXGI_ADAPTER_DESC1 desc;
	adapter->GetDesc1(&desc);

	spdlog::info("{}", desc);

}

Renderer::~Renderer()
{
}
