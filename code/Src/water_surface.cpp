#include "water_surface.h"
#include "lighting_system.h"
#include "shadow_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

WaterSurface::WaterSurface(const Model* model, int nodeIndex)
    : SceneObject(model, nodeIndex) {

}

void WaterSurface::Init() {
    shader = std::make_unique<Shader>(
        "../code/Shaders/water.vert",
        "../code/Shaders/water.frag"
    );

    if (!dudvMap_.LoadFromFile("../resources/water/water_dudv.png", false))
        std::cerr << "[WaterSurface] failed to load dudv map." << std::endl;

    ComputeClipPlanes();
}

void WaterSurface::Update(float dt) {
    // 仅推进时间。dudvOffset 不在池水模式中使用。
    time_ += dt;
    dudvOffset_ = 0.0f;
}

void WaterSurface::Render(const Camera& camera, float aspect)
{

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(aspect);
    glm::vec4 cameraPos = camera.GetPosition();

    if (!shader) return;
    shader->use();
    // --- Set common matrices ---
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec3("cameraPos", glm::vec3(cameraPos));

    // 使用光照系统设置光照参数
    if (lightingSystem_) {
        lightingSystem_->BindLightingToShader(*shader, glm::vec3(cameraPos));
    }

    // SceneObject 提供 ComputeModelMatrix()
    glm::mat4 instanceM = ComputeModelMatrix(); // 目前是单位，可以以后给水面加平移/旋转
    glm::mat4 modelMatrix = instanceM * model->GetModelNodes()[rootNodeIndex].localTransform;
    shader->setMat4("model", modelMatrix);
    // --- 动态扰动参数 ---
    shader->setFloat("time", time_);
    shader->setFloat("dudvOffset", dudvOffset_);
    shader->setFloat("waveStrength", waveStrength_);
    shader->setFloat("normalRippleStrength", normalRippleStrength_);

    // 水面效果参数
    shader->setVec2("waveDirection1", waveDirection1_);
    shader->setVec2("waveDirection2", waveDirection2_);
    shader->setFloat("waveScale1", waveScale1_);
    shader->setFloat("waveScale2", waveScale2_);
    shader->setFloat("fresnelPower", fresnelPower_);
    shader->setFloat("eta", eta_);
    shader->setFloat("deepBoost", deepBoost_);
    shader->setVec3("absorptionCoeff", absorptionCoeff_);
    shader->setVec3("inScatteringColor", inScatteringColor_);
    shader->setVec2("nearFarPlanes", camera.GetNearFarPlanes());

    // --- Bind Water-specific textures ---
    // 0: reflection
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflectionTex_);
    shader->setInt("reflectionTex", 0);

    // 1: refraction
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, refractionTex_);
    shader->setInt("refractionTex", 1);

    // 2: refraction depth texture
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, refractionDepthTex_);
    shader->setInt("refractionDepthTex", 2);

    // 3: dudv
    dudvMap_.Bind(3);
    shader->setInt("dudvMap", 3);

    // --- Render mesh ---
    const ModelNode& node = model->GetModelNodes()[rootNodeIndex];
    if (node.meshIndex >= 0) {
        const auto& mesh = model->GetMeshes()[node.meshIndex];
        mesh->Draw();
    }
}

void WaterSurface::ComputeClipPlanes()
{
    const auto& node = model->GetModelNodes()[rootNodeIndex];
    int meshIndex = node.meshIndex;
    if (meshIndex < 0)
    {
        std::cerr << "[WaterSurface] Error: node has no mesh.\n";
        return;
    }
    // 获取 Mesh
    const auto& mesh = model->GetMeshes()[meshIndex];
    // 获取世界空间的模型矩阵
    glm::mat4 worldM = ComputeModelMatrix() * node.localTransform;
    // 累计所有顶点的 y 值
    float sumY = 0.0f;
    for (const auto& v : mesh->GetVertices())
    {
        glm::vec3 wp = glm::vec3(worldM * glm::vec4(v.Position, 1.0f));
        sumY += wp.y;
    }

    waterHeight_ = sumY / mesh->GetVertices().size();
    // clip-plane for reflection (保留水面上方)
    reflectionClipPlane_ = glm::vec4(0, 1, 0, -waterHeight_);
    // clip-plane for refraction (保留水面下方)
    refractionClipPlane_ = glm::vec4(0, -1, 0, waterHeight_);
}