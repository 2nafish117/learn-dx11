#pragma once

#include "Math.hpp"

class Transform {
public:
    mat4 matrix = DirectX::XMMatrixIdentity();
private:
};

class Camera {

public:
    Transform transform;
    float fov = DirectX::XMConvertToRadians(80.0f);
    float aspect = 16.0f / 9.0f;
	float nearZ = 0.01f;
	float farZ = 100.0f;

public:
    inline mat4 GetView() {
        return DirectX::XMMatrixInverse(nullptr, transform.matrix);
    }

    inline mat4 GetProjection() {
        return DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
    }

private:
};