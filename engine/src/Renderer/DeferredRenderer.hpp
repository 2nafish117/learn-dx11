#pragma once

// windows defines min and max as macros, disable with this, wtf???!! why!!??
#define NOMINMAX
#include <d3d11_1.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <directxmath.h>

#include "Basic.hpp"
#include "Math.hpp"
#include "DX11/DX11Context.hpp"

struct GLFWwindow;

class ShaderAsset;
class MeshAsset;
class TextureAsset;
class Camera;

class StaticMesh;
class VertexShader;
class PixelShader;

class ShaderCompiler;
// class DX11Context;

// #ifdef _DEBUG
// #define DX11_DEBUG
// #endif

class DeferredRenderer {
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
public:
	DeferredRenderer(GLFWwindow* window, std::unique_ptr<DX11Context>&& context);
	~DeferredRenderer();

	// @TODO: temp
	void Render();

	void InitImgui();

	
	void ObtainSwapchainResources();
	void ReleaseSwapchainResources();
	void ResizeSwapchainResources(u32 width, u32 height);
	void HandleResize(u32 width, u32 height);

private:
	GLFWwindow* m_window = nullptr;
	std::unique_ptr<DX11Context> m_context;

	ComPtr<ID3D11RenderTargetView> m_renderTargetView;
	ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	// pipeline states
	ComPtr<ID3D11RasterizerState> m_rasterState;
	ComPtr<ID3D11DepthStencilState> m_depthStencilState;

	D3D11_VIEWPORT m_viewport = {};

	
	std::shared_ptr<MeshAsset> m_quadMeshAsset;
	std::shared_ptr<StaticMesh> m_quadMesh; 

	std::shared_ptr<MeshAsset> m_cubeMeshAsset;
	std::shared_ptr<StaticMesh> m_cubeMesh;

	std::shared_ptr<MeshAsset> m_twoCubeMeshAsset;
	std::shared_ptr<StaticMesh> m_twoCubeMesh; 

	std::shared_ptr<MeshAsset> m_sceneMeshAsset;
	std::shared_ptr<StaticMesh> m_sceneMesh;

	std::shared_ptr<ShaderAsset> m_simpleVertexAsset;
	std::shared_ptr<ShaderAsset> m_simplePixelAsset;

	std::shared_ptr<VertexShader> m_simpleVertex;
	std::shared_ptr<PixelShader> m_simplePixel;

	std::shared_ptr<TextureAsset> m_testTexAsset;

	std::unique_ptr<ShaderCompiler> m_shaderCompiler;

	ComPtr<ID3D11ShaderResourceView> m_testSRV;
	ComPtr<ID3D11SamplerState> m_testSamplerState;

	ComPtr<ID3D11Buffer> m_matrixBuffer;

	ComPtr<ID3D11Buffer> m_pointLightBuffer;

	std::shared_ptr<Camera> m_camera;

	struct MatrixBuffer {
		mat4 ModelToWorld;
		mat4 WorldToView;
		mat4 ViewToProjection;
	};

	struct PointLightBuffer {
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Col;

		byte _padding[8];
	};
};