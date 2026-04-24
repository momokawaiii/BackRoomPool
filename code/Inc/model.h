#pragma once
#include <string>
#include <vector>
#include <memory>
#include "mesh.h"
#include "camera.h"
#include "pbr_material.h"
namespace tinygltf {
    class Model;
    struct Primitive;
    struct Node;
}

struct ModelNode {
    std::string name;
    glm::mat4 localTransform = glm::mat4(1.0f);
    glm::mat4 globalTransform = glm::mat4(1.0f);

    // model.meshes[meshIndex] gives a gltf mesh
    int meshIndex = -1;
    std::vector<int> children;
};

class Model {
public:
    explicit Model(const std::string& path);
    bool Load(); //Load gltf file
    const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return meshes; };
    const std::vector<glm::mat4>& GetMeshLocalTransforms() const { return meshLocalTransforms; };
    const std::vector<std::string>& GetMeshNames() const { return meshNames; };
    const CamInfos& GetModelCamInfos() const;
    const std::vector<ModelNode>& GetModelNodes() const { return modelNodes; };
    const std::vector<Texture2D>& GetTextures() const { return textures; };
    const std::vector<PBRMaterial>& GetMaterials() const { return materials; };

    // 光源相关方法
    bool HasLights() const { return !lights_.empty(); }
    const std::vector<struct Light>& GetLights() const { return lights_; }

    std::vector<std::shared_ptr<Mesh>> GetMeshesByPrefix(const std::string& prefix) const;
    std::vector<glm::mat4> GetTransformsByPrefix(const std::string& prefix) const;

    glm::vec3 GetSceneCenter() const { return (sceneMin_ + sceneMax_) * 0.5f; }
    float GetSceneRadius() const { return glm::length(sceneMax_ - sceneMin_) * 0.5f; }

private:
    std::string filePath;

    //gltf model data
    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<glm::mat4> meshLocalTransforms;
    std::vector<std::string> meshNames;
    std::vector<CamInfos> cameras;
    std::vector<ModelNode> modelNodes;

    // 材质与纹理池
    std::vector<Texture2D> textures;
    std::vector<PBRMaterial> materials;

    // 光源数据
    std::vector<struct Light> lights_;


    // process scene graph nodes recursively
    int ProcessNode(
        const tinygltf::Model& gltf,
        int nodeIndex,
        const glm::mat4& parentTransform
    );

    // process a single mesh primitive and return a Mesh object
    std::shared_ptr<Mesh> ProcessMesh(
        const tinygltf::Model& gltf,
        const tinygltf::Primitive& prim,
        int materialIndex,
        const glm::mat4& worldTransform
    );

    void LoadTextures(const tinygltf::Model& gltf);
    void LoadMaterials(const tinygltf::Model& gltf);
    void LoadCameras(const tinygltf::Model& gltf);
    void LoadLights(const tinygltf::Model& gltf);
    void ProcessLightNode(const tinygltf::Model& gltf, int nodeIndex, const glm::mat4& transform);

    glm::vec3 sceneMin_ = glm::vec3(FLT_MAX);
    glm::vec3 sceneMax_ = glm::vec3(-FLT_MAX);
};