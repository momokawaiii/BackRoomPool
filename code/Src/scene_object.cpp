#include "scene_object.h"
#include "lighting_system.h"
#include "shadow_system.h"

SceneObject::SceneObject(const Model* model, int rootNodeIndex)
    : model(model), rootNodeIndex(rootNodeIndex)
{
    position = glm::vec3(0.0f);
    rotationEulerDeg = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
}

void SceneObject::Init() {
    shader = std::make_unique<Shader>(
        "../code/Shaders/mesh_pbr.vert",
        "../code/Shaders/mesh_pbr.frag"
    );

    // 初始化阴影深度着色器
    shadowShader = std::make_unique<Shader>(
        "../code/Shaders/shadow_depth.vert",
        "../code/Shaders/shadow_depth.frag"
    );
}

void SceneObject::Update(float /*dt*/) {
    // 默认不做动画
}

void SceneObject::Render(const Camera& camera, float aspect)
{
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(aspect);
    glm::vec4 cameraPos = camera.GetPosition();

    shader->use();
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec3("cameraPos", glm::vec3(cameraPos));

    // 使用光照系统设置光照参数
    if (lightingSystem_) {
        lightingSystem_->BindLightingToShader(*shader, glm::vec3(cameraPos));
    }

    // 绑定阴影资源
    if (shadowSystem_) {
        shadowSystem_->BindShadowToShader(*shader);
    }
    glm::mat4 instanceM = ComputeModelMatrix();

    const ModelNode& node = model->GetModelNodes()[rootNodeIndex];
    glm::mat4 globalXform = instanceM * node.localTransform;
    //渲染该节点的 Mesh（如果有的话）
    if (node.meshIndex >= 0) {
        const auto& mesh = model->GetMeshes()[node.meshIndex];
        const PBRMaterial& material = model->GetMaterials()[mesh->GetMaterialIndex()];
        shader->setMat4("model", globalXform);
        shader->SetMaterial(material, *model);
        mesh->Draw();
    }
}

void SceneObject::RenderWithClip(const Camera& camera, float aspect, const glm::vec4& clipPlane)
{
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(aspect);

    shader->use();
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec4("clipPlane", clipPlane);

    // 使用光照系统设置光照参数
    if (lightingSystem_) {
        lightingSystem_->BindLightingToShader(*shader, camera.GetPosition());
    }
    // 绑定阴影资源
    if (shadowSystem_) {
        shadowSystem_->BindShadowToShader(*shader);
    }
    glm::mat4 instanceM = ComputeModelMatrix();

    const ModelNode& node = model->GetModelNodes()[rootNodeIndex];
    glm::mat4 globalXform = instanceM * node.localTransform;
    //渲染该节点的 Mesh（如果有的话）
    if (node.meshIndex >= 0) {
        const auto& mesh = model->GetMeshes()[node.meshIndex];
        const PBRMaterial& material = model->GetMaterials()[mesh->GetMaterialIndex()];
        shader->setMat4("model", globalXform);
        shader->SetMaterial(material, *model);
        mesh->Draw();
    }
}

void SceneObject::RenderShadowDepth() {
    if (!shadowShader || !shadowSystem_) {
        return;
    }

    shadowShader->use();

    // 绑定光空间矩阵
    shadowShader->setMat4("lightSpaceMatrix", shadowSystem_->GetLightSpaceMatrix());

    // 计算模型矩阵
    glm::mat4 instanceM = ComputeModelMatrix();

    const ModelNode& node = model->GetModelNodes()[rootNodeIndex];
    glm::mat4 globalXform = instanceM * node.localTransform;

    //渲染该节点的 Mesh（如果有的话）- 只渲染几何体到深度缓冲
    if (node.meshIndex >= 0) {
        const auto& mesh = model->GetMeshes()[node.meshIndex];
        shadowShader->setMat4("model", globalXform);
        mesh->Draw();
    }
}
// ===== 变换相关 =====

void SceneObject::SetPosition(const glm::vec3& p) {
    position = p;
}

void SceneObject::SetRotationEuler(const glm::vec3& eulerDegrees) {
    rotationEulerDeg = eulerDegrees;
}

void SceneObject::SetScale(const glm::vec3& s) {
    scale = s;
}

glm::mat4 SceneObject::GetModelMatrix() const {
    return ComputeModelMatrix();
}

glm::mat4 SceneObject::ComputeModelMatrix() const {
    glm::mat4 model(1.0f);

    // 1. 平移
    model = glm::translate(model, position);

    // 2. 旋转（注意顺序：Z * Y * X）
    glm::vec3 rad = glm::radians(rotationEulerDeg);
    model = glm::rotate(model, rad.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rad.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rad.z, glm::vec3(0.0f, 0.0f, 1.0f));

    // 3. 缩放
    model = glm::scale(model, scale);

    return model;
}

