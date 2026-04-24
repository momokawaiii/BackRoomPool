#pragma once
#include "stb_image.h"
#include "shader.h"
#include "irenderable.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <memory>

class Skybox : public IRenderable {
public:
    void Init() override;
    void Update(float dt) override;
    void Render(const Camera& camera, float aspect) override;

    // 设置环境立方体贴图
    void SetEnvironmentCubemap(unsigned int cubemap) {
        if (cubemap != 0) {
            cubemapTexture = cubemap;
            useEnvironmentMap = true;
        }
    }

private:
    unsigned int skyboxVAO = 0;
    unsigned int skyboxVBO = 0;
    unsigned int cubemapTexture = 0;
    bool useEnvironmentMap = false;  // 是否使用环境贴图
    unsigned int loadCubemap(std::vector<std::string> faces);
    std::unique_ptr<Shader> shader;
};

