#pragma once

#include "Math.hpp"


class Transform {
public:
    mat4 matrix;
private:
};

class Camera {

public:
    Transform transform;
    float fov = 70.0f;
    float aspect = 16.0f / 9.0f;

public:
    inline mat4 GetView() {
        return DirectX::XMMatrixInverse(nullptr, transform.matrix);
    }

    float4x4 GetProjection() {
        
    }

private:

    
};