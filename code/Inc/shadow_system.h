#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <memory>

class Shader;
class LightingSystem;

class ShadowSystem {
public:
    ShadowSystem(int shadowMapSize = 2048);
    ~ShadowSystem();

    // 禁止拷贝，允许移动
    ShadowSystem(const ShadowSystem&) = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;

    void Init();
    void Update(const LightingSystem* lightingSystem, const glm::vec3& sceneCenter, float sceneRadius);

    // 开始/结束阴影渲染
    void BeginShadowPass();
    void EndShadowPass();

    // 绑定阴影资源到着色器
    void BindShadowToShader(Shader& shader) const;

    // 获取阴影资源
    GLuint GetShadowMap() const { return shadowMap_; }
    const glm::mat4& GetLightSpaceMatrix() const { return lightSpaceMatrix_; }

    // 检查是否有有效的方向光
    bool HasValidDirectionalLight() const { return hasValidLight_; }

    // 阴影参数控制
    void SetShadowBias(float bias) { shadowBias_ = bias; }
    void SetShadowIntensity(float intensity) { shadowIntensity_ = intensity; }
    float GetShadowBias() const { return shadowBias_; }
    float GetShadowIntensity() const { return shadowIntensity_; }

private:
    // 阴影贴图资源
    GLuint shadowFBO_ = 0;
    GLuint shadowMap_ = 0;
    int shadowMapSize_;

    // 光空间变换
    glm::mat4 lightSpaceMatrix_;
    glm::vec3 lightDirection_;
    bool hasValidLight_ = false;

    // 阴影参数
    float shadowBias_ = 0.002f;         // 阴影偏移，防止阴影失真
    float shadowIntensity_ = 0.8f;      // 阴影强度（0=无阴影，1=完全黑）
    float orthoSize_ = 15.0f;           // 正交投影范围
    float nearPlane_ = 0.1f;            // 近裁剪面
    float farPlane_ = 30.0f;            // 远裁剪面

    // 计算光空间矩阵：基于光方向 + 场景中心和半径自适应构建正交体
    void UpdateLightSpaceMatrix(const glm::vec3& lightDir,
        const glm::vec3& sceneCenter,
        float sceneRadius);

    // 资源管理
    void CreateShadowMap();
    void Cleanup();
};