#pragma once
#include "scene_object.h"
#include "texture2d.h"
#include "shader.h"

class WaterSurface : public SceneObject {
public:
    WaterSurface(const Model* model, int nodeIndex);

    void Init() override;
    void Update(float dt) override;
    void Render(const Camera& camera, float aspect) override;

    // Renderer 会通过这些接口塞进 FBO 纹理
    void SetReflectionTexture(unsigned int tex) { reflectionTex_ = tex; }
    void SetRefractionTexture(unsigned int tex) { refractionTex_ = tex; }
    void SetRefractionDepthTexture(unsigned int tex) { refractionDepthTex_ = tex; }

    // 设置光照系统
    void SetLightingSystem(const class LightingSystem* lightingSystem) { lightingSystem_ = lightingSystem; }

    // 设置阴影系统
    void SetShadowSystem(const class ShadowSystem* shadowSystem) { shadowSystem_ = shadowSystem; }

    const float GetWaterHeight() const { return waterHeight_; }
    const glm::vec4& GetReflectionClipPlane() const { return reflectionClipPlane_; }
    const glm::vec4& GetRefractionClipPlane() const { return refractionClipPlane_; }
private:
    // shader
    std::unique_ptr<Shader> shader;

    // 系统指针
    const class ShadowSystem* shadowSystem_ = nullptr;

    // 水面纹理资源
    Texture2D dudvMap_;

    // 来自 renderer 的纹理（不是自己生成）
    GLuint reflectionTex_ = 0;
    GLuint refractionTex_ = 0;
    GLuint refractionDepthTex_ = 0;

    // clip planes
    float waterHeight_ = 0.0f;     // 世界空间的高度
    glm::vec4 reflectionClipPlane_; // y > waterHeight
    glm::vec4 refractionClipPlane_; // y < waterHeight
    void ComputeClipPlanes();

    // 动态扰动参数
    float time_ = 0.0f;
    float dudvOffset_ = 0.0f;
    float waveSpeed_ = 0.05f;
    float waveStrength_ = 0.02f;          // 减少波浪强度
    float normalRippleStrength_ = 0.002f;  // 进一步减少涟漪强度

    // 水面效果参数（liminal pool风格）
    glm::vec2 waveDirection1_ = glm::vec2(1.0f, 0.1f);  // 波浪方向 1（更平缓）
    glm::vec2 waveDirection2_ = glm::vec2(-0.3f, 1.0f); // 波浪方向 2（更平缓）
    float waveScale1_ = 1.5f;              // 波浪缩放 1（减小）
    float waveScale2_ = 2.0f;              // 波浪缩放 2（减小）

    // 水体光学参数（可选扩展）
    const float eta_ = 1.333f; // 水的折射率
    const float fresnelPower_ = 2.5f; // Fresnel 指数
    const glm::vec3 absorptionCoeff_ = glm::vec3(0.03f, 0.05f, 0.09f);
    const glm::vec3 inScatteringColor_ = glm::vec3(0.03f, 0.08f, 0.15f);
    const float deepBoost_ = 0.95f;

    const class LightingSystem* lightingSystem_ = nullptr;  ///< 光照系统指针
};