#pragma once
#include<vector>
#include<memory>
#include"camera.h"
#include"model.h"
#include"framebuffer.h"
#include"skybox.h"
#include "water_surface.h"
#include "lighting_system.h"
#include "shadow_system.h"

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer() = default;

    void Init();
    void Update(float dt);
    void Render();
    Camera& GetCamera() const;
    LightingSystem& GetLightingSystem() const;

    int SCR_WIDTH;
    int SCR_HEIGHT;
    int FB_WIDTH;
    int FB_HEIGHT;
private:
    std::unique_ptr<Camera> camera;
    std::shared_ptr<Model> model;
    std::unique_ptr<LightingSystem> lightingSystem;
    std::unique_ptr<ShadowSystem> shadowSystem;

    std::vector<std::shared_ptr<SceneObject>> renderModules;
    std::shared_ptr<Skybox> skybox;
    std::shared_ptr<WaterSurface> waterSurface;

    Framebuffer reflectionFBO_;
    Framebuffer refractionFBO_;
};