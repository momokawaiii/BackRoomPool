#include "skybox.h"
#include "shader.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

const float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

void Skybox::Init()
{
    // 优先使用HDR环境贴图，如果没有则使用JPG作为回退
    if (!useEnvironmentMap || cubemapTexture == 0) {
        // 使用JPG立方体贴图作为回退
        std::vector<std::string> faces
        {
            "../resources/skybox/right.jpg",   // 0: +X (右)
            "../resources/skybox/left.jpg",    // 1: -X (左)
            "../resources/skybox/top.jpg",     // 2: +Y (上)
            "../resources/skybox/bottom.jpg",  // 3: -Y (下)
            "../resources/skybox/front.jpg",   // 4: +Z (前)
            "../resources/skybox/back.jpg"     // 5: -Z (后)
        };

        cubemapTexture = loadCubemap(faces);
        std::cout << "Skybox using JPG cubemap (fallback), texture ID: " << cubemapTexture << std::endl;
    }
    else {
        // std::cout << "Skybox using HDR environment cubemap, texture ID: " << cubemapTexture << std::endl;
    }
    // Setup skybox VAO/VBO
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    // Load and compile shader
    shader = std::make_unique<Shader>("../code/Shaders/skybox.vert", "../code/Shaders/skybox.frag");
}

void Skybox::Update(float dt)
{
    // No update needed for static skybox
}

void Skybox::Render(const Camera& camera, float aspect)
{
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(aspect);
    glDepthFunc(GL_LEQUAL); // Skybox depth trick
    shader->use();

    // Set uniforms: view and projection matrices
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec4("clipPlane", glm::vec4(0.0f)); // 正常渲染时不裁剪
    shader->setInt("skybox", 0); // 设置纹理单元

    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);

    if (cubemapTexture != 0) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    }
    else {
        // 如果没有立方体贴图，输出警告
        static bool warned = false;
        if (!warned) {
            std::cout << "Warning: No cubemap texture set for Skybox" << std::endl;
            warned = true;
        }
    }

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

unsigned int Skybox::loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cerr << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}