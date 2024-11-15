#pragma once

#include "Basic.hpp"
#include "Math.hpp"
#include "AssetSystem.hpp"

class SceneSystem;
namespace global 
{
	extern SceneSystem* sceneSystem;
}


class Entity;
class CameraEntity;
class StaticMeshEntity;

class Transform;


class RuntimeScene {
public:
	RuntimeScene() {
		camera = std::make_shared<CameraEntity>();
		staticMeshEntity0 = std::make_shared<StaticMeshEntity>();
		staticMeshEntity1 = std::make_shared<StaticMeshEntity>();
	}
public:
	std::shared_ptr<CameraEntity> camera;
	std::shared_ptr<StaticMeshEntity> staticMeshEntity0;
	std::shared_ptr<StaticMeshEntity> staticMeshEntity1;
};


class SceneSystem {
	class Entity;
	class CameraEntity;
	class StaticMeshEntity;

public:
	SceneSystem() {
		spdlog::info("SceneSystem init");
	}
	~SceneSystem() {
		spdlog::info("SceneSystem de-init");
	}

	std::shared_ptr<RuntimeScene> runtimeScene;
};


using EntityID = u32;


class Transform {
public:
	mat4 matrix = DirectX::XMMatrixIdentity();
private:
};


class Entity {
public:
	Transform xform;
};


class CameraEntity : public Entity {
public:
	float fov = DirectX::XMConvertToRadians(80.0f);
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.01f;
	float farZ = 100.0f;

public:
    inline mat4 GetView() {
        return DirectX::XMMatrixInverse(nullptr, xform.matrix);
    }

    inline mat4 GetProjection() {
        return DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
    }

private:
};


class StaticMeshEntity : public Entity {
public:
	MeshID meshAsset = { 0 };
	// @TODO: hardcoded!!!!!!!!!!!!!!!!!!!!!!
	ShaderID vertShaderAsset = { 0 };
	ShaderID pixShaderAsset = { 1 };

	TextureID texAsset = { 0 };
private:
};