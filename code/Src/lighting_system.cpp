#include "lighting_system.h"
#include "model.h"
#include "shader.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

// 常量定义 - 提高分辨率以改善画质
const int CUBEMAP_SIZE = 1024;  // 从512提升到1024
const int IRRADIANCE_SIZE = 64; // 辐照度图分辨率
const int PREFILTER_SIZE = 256; // 预滤波图分辨率

LightingSystem::LightingSystem() {
}

LightingSystem::~LightingSystem() {
    // 清理OpenGL资源
    if (environmentLighting_.environmentMap != 0) {
        glDeleteTextures(1, &environmentLighting_.environmentMap);
    }
    if (environmentLighting_.irradianceMap != 0) {
        glDeleteTextures(1, &environmentLighting_.irradianceMap);
    }
    if (environmentLighting_.prefilterMap != 0) {
        glDeleteTextures(1, &environmentLighting_.prefilterMap);
    }
    if (environmentLighting_.brdfLUT != 0) {
        glDeleteTextures(1, &environmentLighting_.brdfLUT);
    }
    if (cubeVAO_ != 0) {
        glDeleteVertexArrays(1, &cubeVAO_);
    }
    if (quadVAO_ != 0) {
        glDeleteVertexArrays(1, &quadVAO_);
    }
}

void LightingSystem::Init() {
    // 设置默认环境光
    environmentLighting_.ambientColor = glm::vec3(0.02f, 0.02f, 0.02f);
    environmentLighting_.ambientIntensity = 0.5f;  // 降低环境光强度

    // 添加默认主光源（太阳光）
    // Light mainLight;
    // mainLight.type = LightType::DIRECTIONAL;
    // mainLight.direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
    // mainLight.color = glm::vec3(1.0f, 0.9f, 0.8f);
    // mainLight.intensity = 0.8f;  // 降低默认强度

    // lights_.push_back(mainLight);
}

void LightingSystem::LoadHDREnvironment(const std::string& hdrPath) {
    std::cout << "Loading HDR environment: " << hdrPath << std::endl;

    // 加载HDR纹理
    GLuint hdrTexture = LoadHDRTexture(hdrPath);
    if (hdrTexture == 0) {
        std::cerr << "Failed to load HDR texture: " << hdrPath << std::endl;
        return;
    }

    // 重新启用HDR转换功能
    GLuint cubemapTexture = ConvertHDRToCubemap(hdrTexture);
    if (cubemapTexture != 0) {
        environmentLighting_.hasHDREnvironment = true;
        environmentLighting_.environmentMap = cubemapTexture;
        // std::cout << "HDR environment converted to cubemap successfully" << std::endl;

        // 生成IBL预计算贴图
        GenerateIrradianceMap(cubemapTexture);
        GeneratePrefilterMap(cubemapTexture);
        GenerateBRDFLUT();

        // std::cout << "IBL precomputation completed" << std::endl;
    }
    else {
        std::cerr << "Failed to convert HDR to cubemap" << std::endl;
    }

    // 清理临时HDR纹理
    glDeleteTextures(1, &hdrTexture);
}GLuint LightingSystem::LoadHDRTexture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float* data = stbi_loadf(path.c_str(), &width, &height, &nrComponents, 0);

    if (!data) {
        std::cerr << "Failed to load HDR image: " << path << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

void LightingSystem::AddLight(const Light& light) {
    lights_.push_back(light);
}

void LightingSystem::LoadLightsFromModel(const Model* model) {
    if (!model || !model->HasLights()) {
        std::cout << "No lights found in glTF model" << std::endl;
        return;
    }
    const auto& modelLights = model->GetLights();
    for (auto light : modelLights) {
        light.intensity *= gltf_intensityScale;  // 应用强度控制
        AddLight(light);
        // std::cout << "Added light with scaled intensity: " << light.intensity << std::endl;
    }
}
glm::vec3 LightingSystem::GetMainLightDirection() const {
    // 返回第一个平行光的方向，如果没有则返回默认值
    for (const auto& light : lights_) {
        if (light.type == LightType::DIRECTIONAL) {
            return light.direction;
        }
    }
    return glm::vec3(-0.2f, -1.0f, -0.3f); // 默认方向
}

glm::vec3 LightingSystem::GetMainLightColor() const {
    // 返回第一个平行光的颜色，如果没有则返回默认值
    for (const auto& light : lights_) {
        if (light.type == LightType::DIRECTIONAL) {
            return light.color * light.intensity;
        }
    }
    return glm::vec3(1.0f, 0.9f, 0.8f); // 默认颜色
}

void LightingSystem::BindLightingToShader(Shader& shader, const glm::vec3& cameraPos) const {
    shader.use();

    // 设置环境光
    shader.setVec3("ambientLight", environmentLighting_.ambientColor * environmentLighting_.ambientIntensity);

    // 设置相机位置
    shader.setVec3("cameraPos", cameraPos);

    // 设置光源数量
    int numLights = std::min(static_cast<int>(lights_.size()), 32); // 限制最大光源数量
    shader.setInt("numLights", numLights);

    // 设置各个光源
    for (int i = 0; i < numLights; ++i) {
        const Light& light = lights_[i];
        std::string lightBase = "lights[" + std::to_string(i) + "]";

        shader.setInt(lightBase + ".type", static_cast<int>(light.type));
        shader.setVec3(lightBase + ".position", light.position);
        shader.setVec3(lightBase + ".direction", light.direction);
        shader.setVec3(lightBase + ".color", light.color);
        shader.setFloat(lightBase + ".intensity", light.intensity * lightIntensityMultiplier_);
        shader.setFloat(lightBase + ".constant", light.constant);
        shader.setFloat(lightBase + ".linear", light.linear);
        shader.setFloat(lightBase + ".quadratic", light.quadratic);
        shader.setFloat(lightBase + ".innerConeAngle", light.innerConeAngle);
        shader.setFloat(lightBase + ".outerConeAngle", light.outerConeAngle);
    }

    // IBL纹理绑定
    if (environmentLighting_.hasHDREnvironment &&
        environmentLighting_.irradianceMap != 0 &&
        environmentLighting_.prefilterMap != 0 &&
        environmentLighting_.brdfLUT != 0) {

        shader.setInt("hasIBL", 1);

        // 设置IBL强度参数
        shader.setFloat("iblIntensity", environmentLighting_.iblIntensity);
        shader.setFloat("diffuseIBLIntensity", environmentLighting_.diffuseIntensity);
        shader.setFloat("specularIBLIntensity", environmentLighting_.specularIntensity);

        // 绑定IBL纹理到纹理单元
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environmentLighting_.irradianceMap);
        shader.setInt("irradianceMap", 10);

        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environmentLighting_.prefilterMap);
        shader.setInt("prefilterMap", 11);

        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, environmentLighting_.brdfLUT);
        shader.setInt("brdfLUT", 12);

        // 重置纹理单元到默认值
        glActiveTexture(GL_TEXTURE0);
    }
    else {
        shader.setInt("hasIBL", 0);
    }

    // 为了兼容现有代码，也设置旧的uniform
    if (!lights_.empty() && lights_[0].type == LightType::DIRECTIONAL) {
        shader.setVec3("lightDir", lights_[0].direction);
        shader.setVec3("lightColor", lights_[0].color * lights_[0].intensity * lightIntensityMultiplier_);
    }
}

void LightingSystem::Update(float deltaTime) {
    // 这里可以添加动态光源的更新逻辑
    // 例如：移动光源、闪烁效果等
}

GLuint LightingSystem::CreateCubemap(int width, int height, GLenum internalFormat) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
            width, height, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // 根据纹理尺寸选择适合的过滤方式
    if (width > 256) {
        // 高分辨率纹理使用mipmap过滤
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
    else {
        // 低分辨率纹理使用线性过滤
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    return textureID;
}

void LightingSystem::GenerateIrradianceMap(GLuint environmentMap) {
    // std::cout << "Generating irradiance map..." << std::endl;

    // 创建辐照度图立方体贴图 - 提高分辨率
    environmentLighting_.irradianceMap = CreateCubemap(IRRADIANCE_SIZE, IRRADIANCE_SIZE, GL_RGB16F);

    // 创建和编译辐照度卷积着色器
    Shader irradianceShader("../code/Shaders/cubemap.vert", "../code/Shaders/irradiance_convolution.frag");

    // 设置投影矩阵（90度FOV用于立方体贴图）
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // 创建帧缓冲用于渲染
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IRRADIANCE_SIZE, IRRADIANCE_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // 设置着色器
    irradianceShader.use();
    irradianceShader.setInt("environmentMap", 0);
    irradianceShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

    glViewport(0, 0, IRRADIANCE_SIZE, IRRADIANCE_SIZE); // 配置视口到辐照度图尺寸
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradianceShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentLighting_.irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 重置视口 (需要获取当前窗口尺寸)
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        int width, height;
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
    }

    // 清理
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    // std::cout << "Irradiance map generated successfully" << std::endl;
}

void LightingSystem::GeneratePrefilterMap(GLuint environmentMap) {
    // std::cout << "Generating prefilter map..." << std::endl;

    // 创建预滤波立方体贴图
    environmentLighting_.prefilterMap = CreateCubemap(PREFILTER_SIZE, PREFILTER_SIZE, GL_RGB16F);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentLighting_.prefilterMap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // 创建和编译预滤波着色器
    Shader prefilterShader("../code/Shaders/cubemap.vert", "../code/Shaders/prefilter.frag");

    // 设置投影矩阵
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // 创建帧缓冲
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, PREFILTER_SIZE, PREFILTER_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    prefilterShader.use();
    prefilterShader.setInt("environmentMap", 0);
    prefilterShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // 根据mip级别调整帧缓冲尺寸
        unsigned int mipWidth = static_cast<unsigned int>(PREFILTER_SIZE * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(PREFILTER_SIZE * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.setFloat("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilterShader.setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentLighting_.prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 重置视口
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        int width, height;
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
    }

    // 清理
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    // std::cout << "Prefilter map generated successfully" << std::endl;
}

void LightingSystem::GenerateBRDFLUT() {
    // std::cout << "Generating BRDF LUT..." << std::endl;

    // 创建2D纹理用于BRDF LUT
    glGenTextures(1, &environmentLighting_.brdfLUT);
    glBindTexture(GL_TEXTURE_2D, environmentLighting_.brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 创建帧缓冲
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, environmentLighting_.brdfLUT, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    glViewport(0, 0, 512, 512);
    Shader brdfShader("../code/Shaders/brdf.vert", "../code/Shaders/brdf.frag");
    brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 重置视口
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        int width, height;
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
    }

    // 清理
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    // std::cout << "BRDF LUT generated successfully" << std::endl;
}

void LightingSystem::RenderCube() {
    // 初始化立方体VAO（如果尚未创建）
    if (cubeVAO_ == 0) {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
             // bottom face
             -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
              1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
              1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
              1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
             -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             // top face
             -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
              1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
              1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
              1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };

        GLuint cubeVBO;
        glGenVertexArrays(1, &cubeVAO_);
        glGenBuffers(1, &cubeVBO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindVertexArray(cubeVAO_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }

    // 渲染立方体
    glBindVertexArray(cubeVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void LightingSystem::RenderQuad() {
    // 初始化四边形VAO（如果尚未创建）
    if (quadVAO_ == 0) {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        GLuint quadVBO;
        glGenVertexArrays(1, &quadVAO_);
        glGenBuffers(1, &quadVBO);

        glBindVertexArray(quadVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }

    // 渲染四边形
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

GLuint LightingSystem::ConvertHDRToCubemap(GLuint hdrTexture) {
    // std::cout << "Converting HDR equirectangular to cubemap using GPU..." << std::endl;

    // 创建高分辨率立方体贴图纹理
    GLuint cubemapTexture = CreateCubemap(CUBEMAP_SIZE, CUBEMAP_SIZE, GL_RGB16F);

    // 设置投影和视图矩阵
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // -Z
    };

    // 创建和编译equirectangular到cubemap的着色器
    Shader equirectangularToCubemapShader("../code/Shaders/cubemap.vert", "../code/Shaders/equirectangular_to_cubemap.frag");

    // 创建帧缓冲
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CUBEMAP_SIZE, CUBEMAP_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // 设置着色器uniform
    equirectangularToCubemapShader.use();
    equirectangularToCubemapShader.setInt("equirectangularMap", 0);
    equirectangularToCubemapShader.setMat4("projection", captureProjection);

    // 绑定HDR纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, CUBEMAP_SIZE, CUBEMAP_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    // 为每个立方体面渲染
    for (unsigned int i = 0; i < 6; ++i) {
        equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube(); // 渲染立方体几何
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 生成mipmap以改善质量
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // 重置视口
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        int width, height;
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
    }

    // 清理资源
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    // std::cout << "HDR to cubemap conversion completed using GPU, texture ID: " << cubemapTexture << std::endl;


    return cubemapTexture;
}