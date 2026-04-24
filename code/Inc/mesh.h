//https://learnopengl.com/Model-Loading/Mesh
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>



struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    glm::vec2 TexCoords2;
    float Handedness;
};


class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        int materialIndex = -1);

    void Draw() const;

    const std::vector<Vertex>& GetVertices() const { return vertices; }
    const std::vector<unsigned int>& GetIndices() const { return indices; }
    int GetMaterialIndex() const { return materialIndex; }

private:
    unsigned int VAO, VBO, EBO;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    int materialIndex;

    void setupMesh();
};