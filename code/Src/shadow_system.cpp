#include "shadow_system.h"
#include "lighting_system.h"
#include "shader.h"
#include <iostream>
#include <algorithm>
#include <cmath>

ShadowSystem::ShadowSystem(int shadowMapSize)
    : shadowMapSize_(shadowMapSize) {
}

ShadowSystem::~ShadowSystem() {
    Cleanup();
}

void ShadowSystem::Init() {
    CreateShadowMap();
}

void ShadowSystem::Update(const LightingSystem* lightingSystem, const glm::vec3& sceneCenter, float sceneRadius) {
    hasValidLight_ = false;

    if (!lightingSystem) {
        return;
    }

    // 查找主方向光
    const auto& lights = lightingSystem->GetLights();
    for (const auto& light : lights) {
        if (light.type == LightType::DIRECTIONAL) {
            lightDirection_ = glm::normalize(light.direction);
            hasValidLight_ = true;
            UpdateLightSpaceMatrix(lightDirection_, sceneCenter, sceneRadius);
            break;
        }
    }

    if (!hasValidLight_) {
        std::cout << "Warning: No directional light found for shadow casting" << std::endl;
    }
}

void ShadowSystem::BeginShadowPass() {
    if (!hasValidLight_ || shadowFBO_ == 0) {
        std::cout << "[ShadowSystem] Shadow pass skipped: hasValidLight_=" << hasValidLight_ << ", shadowFBO_=" << shadowFBO_ << std::endl;
        return;
    }

    // 绑定阴影帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glViewport(0, 0, shadowMapSize_, shadowMapSize_);

    // 清除深度缓冲
    glClear(GL_DEPTH_BUFFER_BIT);

    // 启用深度测试，禁用颜色写入
    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // 设置深度偏移以减少阴影失真
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

void ShadowSystem::EndShadowPass() {
    if (!hasValidLight_) {
        return;
    }

    // 恢复渲染状态
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // 解绑帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowSystem::BindShadowToShader(Shader& shader) const {
    shader.use();

    if (!hasValidLight_) {
        shader.setInt("hasShadow", 0);
        return;
    }

    // 绑定阴影相关uniform
    shader.setInt("hasShadow", 1);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix_);
    shader.setFloat("shadowBias", shadowBias_);
    shader.setFloat("shadowIntensity", shadowIntensity_);

    // 绑定阴影贴图到纹理单元13（避免与IBL冲突）
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, shadowMap_);
    shader.setInt("shadowMap", 13);

    // 重置到默认纹理单元
    glActiveTexture(GL_TEXTURE0);
}

void ShadowSystem::UpdateLightSpaceMatrix(const glm::vec3& lightDir,
    const glm::vec3& sceneCenter,
    float sceneRadius) {
    // 归一化方向并缓存
    glm::vec3 dir = glm::normalize(lightDir);
    lightDirection_ = dir;

    // 防止半径为零导致的奇异情况
    float radius = std::max(sceneRadius, 1.0f);

    // 将“光源相机”放在场景中心沿光线反方向若干个半径处
    glm::vec3 lightPos = sceneCenter - dir * radius * 2.0f;

    // 选择一个与光方向不共线的 up 向量
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(dir, up)) > 0.95f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

    // 基于场景半径自适应计算正交体大小和 near/far，并写回成员，便于调试或后续 UI 调整
    orthoSize_ = radius * 1.5f;          // 略大于场景半径
    nearPlane_ = 0.1f;
    // far 至少要覆盖从光源到场景背面，预留倍数避免裁剪
    farPlane_ = std::max(radius * 4.0f, nearPlane_ + 1.0f);

    glm::mat4 lightProjection = glm::ortho(
        -orthoSize_, orthoSize_,
        -orthoSize_, orthoSize_,
        nearPlane_, farPlane_
    );

    lightSpaceMatrix_ = lightProjection * lightView;
}

void ShadowSystem::CreateShadowMap() {
    // 我们需要生成一张深度贴图(Depth Map). 深度贴图是从光的透视图里渲染的深度纹理,用它计算阴影.
    // 因为我们需要将场景的渲染结果存储到一个纹理中,我们将再次需要帧缓冲

    // 清理旧资源
    Cleanup();

    // 为渲染的深度贴图创建一个帧缓冲对象
    glGenFramebuffers(1, &shadowFBO_);

    // 创建一个2D纹理,提供给帧缓冲的深度缓冲使用
    glGenTextures(1, &shadowMap_);
    glBindTexture(GL_TEXTURE_2D, shadowMap_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowMapSize_, shadowMapSize_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // 设置边界颜色为1.0（最大深度），这样超出阴影贴图范围的区域不会产生阴影
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // 把生成的深度纹理作为帧缓冲的深度缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap_, 0);
    // 不需要颜色缓冲
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // 检查帧缓冲完整性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Shadow framebuffer not complete!" << std::endl;
    }

    // 解绑帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ShadowSystem::Cleanup() {
    if (shadowFBO_ != 0) {
        glDeleteFramebuffers(1, &shadowFBO_);
        shadowFBO_ = 0;
    }

    if (shadowMap_ != 0) {
        glDeleteTextures(1, &shadowMap_);
        shadowMap_ = 0;
    }
}