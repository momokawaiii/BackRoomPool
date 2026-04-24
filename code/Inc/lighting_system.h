#pragma once
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

// 光源类型枚举
enum class LightType {
    DIRECTIONAL = 0,
    POINT = 1,
    SPOT = 2
};

// 光源结构体
struct Light {
    LightType type;
    glm::vec3 position;    // 点光源和聚光灯位置，平行光忽略
    glm::vec3 direction;   // 平行光和聚光灯方向，点光源忽略
    glm::vec3 color;       // 光源颜色/强度
    float intensity;       // 光照强度

    // 点光源衰减参数
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    // 聚光灯参数
    float innerConeAngle = 0.0f;  // 内锥角（弧度）
    float outerConeAngle = 0.0f;  // 外锥角（弧度）

    Light() = default;
    Light(LightType t, const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& col, float intens)
        : type(t), position(pos), direction(dir), color(col), intensity(intens) {
    }
};

// 环境光照结构体
struct EnvironmentLighting {
    glm::vec3 ambientColor = glm::vec3(0.03f);
    float ambientIntensity = 1.0f;

    // IBL强度控制参数
    float iblIntensity = 1.0f;        // IBL整体强度倍数
    float diffuseIntensity = 1.0f;    // 漫反射IBL强度
    float specularIntensity = 1.0f;   // 镜面反射IBL强度

    // HDR环境贴图相关
    bool hasHDREnvironment = false;
    GLuint environmentMap = 0;
    GLuint irradianceMap = 0;   // 预计算的漫反射辐照度图
    GLuint prefilterMap = 0;    // 预计算的镜面反射预滤波图
    GLuint brdfLUT = 0;         // BRDF查找纹理
};

class LightingSystem {
public:
    LightingSystem();
    ~LightingSystem();
    // 禁止拷贝，允许移动
    LightingSystem(const LightingSystem&) = delete;
    LightingSystem& operator=(const LightingSystem&) = delete;


    // 初始化光照系统
    void Init();

    // 从HDR文件加载环境光照
    void LoadHDREnvironment(const std::string& hdrPath);

    // 添加光源
    void AddLight(const Light& light);

    // 从glTF模型加载光源
    void LoadLightsFromModel(const class Model* model);

    // 获取光照信息
    const std::vector<Light>& GetLights() const { return lights_; }
    const EnvironmentLighting& GetEnvironmentLighting() const { return environmentLighting_; }

    // 获取主要平行光（用于兼容现有的简单光照接口）
    glm::vec3 GetMainLightDirection() const;
    glm::vec3 GetMainLightColor() const;

    // 绑定光照数据到着色器
    void BindLightingToShader(class Shader& shader, const glm::vec3& cameraPos) const;

    // 更新动态光源（如果有的话）
    void Update(float deltaTime);

    // 设置光源强度倍数
    void SetLightIntensityMultiplier(float multiplier) { lightIntensityMultiplier_ = multiplier; }
    float GetLightIntensityMultiplier() const { return lightIntensityMultiplier_; }

    // IBL强度控制方法
    void SetIBLIntensity(float intensity) { environmentLighting_.iblIntensity = intensity; }
    void SetDiffuseIBLIntensity(float intensity) { environmentLighting_.diffuseIntensity = intensity; }
    void SetSpecularIBLIntensity(float intensity) { environmentLighting_.specularIntensity = intensity; }
    float GetIBLIntensity() const { return environmentLighting_.iblIntensity; }

    // 环境光强度控制
    void SetAmbientIntensity(float intensity) { environmentLighting_.ambientIntensity = intensity; }
    float GetAmbientIntensity() const { return environmentLighting_.ambientIntensity; }

    // 获取环境立方体贴图
    GLuint GetEnvironmentCubemap() const { return environmentLighting_.environmentMap; }

private:
    std::vector<Light> lights_;
    EnvironmentLighting environmentLighting_;
    float lightIntensityMultiplier_ = 0.3f;  // 光照强度控制系数
    float gltf_intensityScale = 0.002f;  // 强度控制系数，大幅降低glTF光源强度

    // HDR环境贴图处理
    GLuint LoadHDRTexture(const std::string& path);
    GLuint ConvertHDRToCubemap(GLuint hdrTexture);
    void GenerateIrradianceMap(GLuint environmentMap);
    void GeneratePrefilterMap(GLuint environmentMap);
    void GenerateBRDFLUT();

    // 辅助函数
    GLuint CreateCubemap(int width, int height, GLenum internalFormat);
    void RenderCube();
    void RenderQuad();

    // 几何体VAO
    GLuint cubeVAO_ = 0;
    GLuint quadVAO_ = 0;
};