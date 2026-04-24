#pragma once
#include <glm/glm.hpp>
#include "camera.h"

class IRenderable {
public:
    virtual ~IRenderable() = default;
    virtual void Init() = 0;
    virtual void Update(float dt) = 0;
    virtual void Render(const Camera& camera, float aspect) = 0;
};