#include "renderer.h"
#include "utils.h"
#include <GLFW/glfw3.h>
Renderer::Renderer(int width, int height)
    :SCR_WIDTH(width), SCR_HEIGHT(height), FB_WIDTH(width), FB_HEIGHT(height),
    reflectionFBO_(),
    refractionFBO_() {
    model = std::make_shared<Model>("../resources/blender/pools.glb");
    model->GetModelCamInfos();
    camera = std::make_unique<Camera>(model->GetModelCamInfos());
    // 初始化光照系统
    lightingSystem = std::make_unique<LightingSystem>();

    // 初始化阴影系统
    shadowSystem = std::make_unique<ShadowSystem>(2048);

    /*more render modules can be added here*/
    skybox = std::make_shared<Skybox>();

    for (int nodeIndex = 0; nodeIndex < model->GetModelNodes().size(); ++nodeIndex) {
        const ModelNode& node = model->GetModelNodes()[nodeIndex];
        if (node.meshIndex >= 0) { // 仅渲染包含 Mesh 的节点
            if (matchPrefixIgnoreCase(node.name, "water"))
                waterSurface = std::make_shared<WaterSurface>(model.get(), nodeIndex);
            else
                renderModules.push_back(std::make_shared<SceneObject>(model.get(), nodeIndex));
        }
    }
}

void Renderer::Init() {
    GLFWwindow* win = glfwGetCurrentContext();
    glfwGetFramebufferSize(win, &FB_WIDTH, &FB_HEIGHT);

    // 初始化光照系统
    lightingSystem->Init();

    // 加载HDR环境贴图
    lightingSystem->LoadHDREnvironment("../resources/hdr/fog-moon_2K.hdr");

    // 从模型加载光源
    lightingSystem->LoadLightsFromModel(model.get());

    // 初始化阴影系统
    shadowSystem->Init();

    reflectionFBO_.Init(FB_WIDTH, FB_HEIGHT);
    refractionFBO_.Init(FB_WIDTH, FB_HEIGHT);
    // Skybox
    if (skybox) {
        // 设置环境立方体贴图（如果可用）
        GLuint envCubemap = lightingSystem->GetEnvironmentCubemap();
        if (envCubemap != 0) {
            skybox->SetEnvironmentCubemap(envCubemap);
        }
        skybox->Init();
    }
    // Static SceneObjects
    for (auto& module : renderModules) {
        module->Init();
        // 如果是SceneObject，设置光照系统和阴影系统
        if (SceneObject* sceneObj = dynamic_cast<SceneObject*>(module.get())) {
            sceneObj->SetLightingSystem(lightingSystem.get());
            sceneObj->SetShadowSystem(shadowSystem.get());
        }
    }
    // Water Surface
    if (waterSurface) {
        waterSurface->Init();
        waterSurface->SetLightingSystem(lightingSystem.get());
        waterSurface->SetShadowSystem(shadowSystem.get());
        waterSurface->SetReflectionTexture(reflectionFBO_.GetColorTexture());
        waterSurface->SetRefractionTexture(refractionFBO_.GetColorTexture());
        waterSurface->SetRefractionDepthTexture(refractionFBO_.GetDepthTexture());
    }
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND); // enable transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Update(float dt) {
    // 更新光照系统
    lightingSystem->Update(dt);
    shadowSystem->Update(lightingSystem.get(), model->GetSceneCenter(), model->GetSceneRadius());

    if (waterSurface) {
        waterSurface->Update(dt);
    }

    GLFWwindow* win = glfwGetCurrentContext();
    int fbW, fbH;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    if (fbW != FB_WIDTH || fbH != FB_HEIGHT) {
        FB_WIDTH = fbW;
        FB_HEIGHT = fbH;
        reflectionFBO_.Resize(FB_WIDTH, FB_HEIGHT);
        refractionFBO_.Resize(FB_WIDTH, FB_HEIGHT);
        if (waterSurface) {
            waterSurface->SetReflectionTexture(reflectionFBO_.GetColorTexture());
            waterSurface->SetRefractionTexture(refractionFBO_.GetColorTexture());
            waterSurface->SetRefractionDepthTexture(refractionFBO_.GetDepthTexture());
        }
    }
}

void Renderer::Render() {
    float aspect = static_cast<float>(FB_WIDTH) / static_cast<float>(FB_HEIGHT);

    // --------- Shadow Pass ---------
    // 只有当有有效方向光时才渲染阴影
    if (shadowSystem->HasValidDirectionalLight()) {
        shadowSystem->BeginShadowPass();

        // 渲染所有投射阴影的物体（除水面外的SceneObject）
        for (auto& obj : renderModules) {
            obj->RenderShadowDepth();
        }

        shadowSystem->EndShadowPass();
    }

    // --------- Reflection Pass ---------
    glEnable(GL_CLIP_DISTANCE0);
    reflectionFBO_.Bind();
    glViewport(0, 0, reflectionFBO_.GetWidth(), reflectionFBO_.GetHeight());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Camera reflectedCam = camera->MakeReflectedCamera(waterSurface->GetWaterHeight());
    glm::vec4 clipUp = waterSurface->GetReflectionClipPlane();

    // Skybox
    skybox->Render(reflectedCam, aspect);

    // All SceneObjects (except water) - 反射pass不需要阴影
    for (auto& obj : renderModules) {
        obj->RenderWithClip(reflectedCam, aspect, clipUp);
    }

    // --------- Refraction Pass ---------
    refractionFBO_.Bind();
    glViewport(0, 0, refractionFBO_.GetWidth(), refractionFBO_.GetHeight());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::vec4 clipDown = waterSurface->GetRefractionClipPlane();

    skybox->Render(*camera, aspect);

    // 折射pass需要阴影信息（水下物体的阴影）
    for (auto& obj : renderModules) {
        obj->RenderWithClip(*camera, aspect, clipDown);
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    // --------- Final Scene Pass ---------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, FB_WIDTH, FB_HEIGHT);
    glDisable(GL_CLIP_DISTANCE0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    skybox->Render(*camera, aspect);

    // 最终pass - 渲染所有物体（包含阴影）
    for (auto& obj : renderModules) {
        obj->Render(*camera, aspect);
    }

    // 最后渲染水面（透明）
    waterSurface->Render(*camera, aspect);
}

Camera& Renderer::GetCamera() const {
    return *camera;
}

LightingSystem& Renderer::GetLightingSystem() const {
    return *lightingSystem;
}