#include "SceneSystem.hpp"

namespace global 
{
	SceneSystem* sceneSystem = nullptr;
}

RuntimeScene::RuntimeScene()
{
	camera = std::make_shared<CameraEntity>();
	camera->xform.matrix = DirectX::XMMatrixTranslation(0, 0, -3);

	staticMeshEntity0 = std::make_shared<StaticMeshEntity>();
	staticMeshEntity0->xform.matrix = DirectX::XMMatrixIdentity();
	staticMeshEntity0->meshAsset = {0};
	staticMeshEntity0->vertShaderAsset = {0};
	staticMeshEntity0->pixShaderAsset = {1};
	staticMeshEntity0->texAsset = {0};

	staticMeshEntity1 = std::make_shared<StaticMeshEntity>();
	staticMeshEntity1->xform.matrix = DirectX::XMMatrixIdentity();
	staticMeshEntity1->meshAsset = {0};
	staticMeshEntity1->vertShaderAsset = {0};
	staticMeshEntity1->pixShaderAsset = {1};
	staticMeshEntity1->texAsset = {0};
}
