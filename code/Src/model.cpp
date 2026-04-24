#include "model.h"
#include "lighting_system.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

Model::Model(const std::string& path) :filePath(path) {
    Load();
    // for (PBRMaterial& mat : materials) {
    //     std::cout << "Material: " << mat.name << " " << (int)mat.alphaMode << std::endl;
    // }
    return;
}

const CamInfos& Model::GetModelCamInfos() const {
    static CamInfos defaultCam;
    if (cameras.empty()) return defaultCam;
    return cameras[0];
}

std::vector<std::shared_ptr<Mesh>> Model::GetMeshesByPrefix(const std::string& prefix) const {
    std::vector<std::shared_ptr<Mesh>> result;
    for (size_t i = 0; i < meshNames.size(); i++) {
        const std::string& name = meshNames[i];
        if (name.rfind(prefix, 0) == 0) {
            result.push_back(meshes[i]);
        }
    }
    return result;
}

std::vector<glm::mat4> Model::GetTransformsByPrefix(const std::string& prefix) const {
    std::vector<glm::mat4> result;
    for (size_t i = 0; i < meshNames.size(); i++) {
        const std::string& name = meshNames[i];
        if (name.rfind(prefix, 0) == 0) {
            result.push_back(meshLocalTransforms[i]);
        }
    }
    return result;
}


bool Model::Load() {
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string err;
    std::string warn;
    bool ret = false;
    if (filePath.size() >= 4 &&
        filePath.substr(filePath.size() - 4) == ".glb") {
        // Binary glTF
        std::cout << "Loading glb file: " << filePath << std::endl;
        ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath);
    }
    else {
        // ASCII glTF
        std::cout << "Loading glTF file: " << filePath << std::endl;
        ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
    }
    if (!warn.empty()) { std::cerr << "Warning: " << warn << std::endl; }
    if (!err.empty()) { std::cerr << "Error: " << err << std::endl; }
    if (!ret) {
        std::cerr << "Failed to load glTF: " << filePath << std::endl;
        return false;
    }

    meshes.clear();
    meshLocalTransforms.clear();
    meshNames.clear();

    const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene];
    for (int rootNodeIndex : scene.nodes) {
        ProcessNode(gltfModel, rootNodeIndex, glm::mat4(1.0f));
    }

    LoadMaterials(gltfModel);
    LoadTextures(gltfModel);
    LoadCameras(gltfModel);
    LoadLights(gltfModel);

    return true;
}

//* Compose transformation matrix from translation, rotation, scale
static glm::mat4 ComposeTRS(const tinygltf::Node& node) {
    glm::vec3 translation(0.0f);
    if (node.translation.size() == 3) {
        translation = glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]));
    }

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4) {
        rotation = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }

    glm::vec3 scale(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]));
    }

    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

    return T * R * S;
}

int Model::ProcessNode(const tinygltf::Model& gltf, int nodeIndex, const glm::mat4& parentTransform) {
    const tinygltf::Node& node = gltf.nodes[nodeIndex];

    // 1. 构造 ModelNode
    ModelNode modelNode;
    modelNode.name = node.name;
    modelNode.localTransform = ComposeTRS(node);
    modelNode.globalTransform = parentTransform * modelNode.localTransform;

    // 2. 处理 Mesh
    if (node.mesh >= 0 && node.mesh < gltf.meshes.size()) {
        const tinygltf::Mesh& gltfMesh = gltf.meshes[node.mesh];
        const size_t primitiveCount = gltfMesh.primitives.size();

        // 一个 glTF Mesh 可能包含多个 Primitive
        // Mesh x Primitive -> meshIndex
        for (const auto& prim : gltfMesh.primitives) {
            int materialIndex = prim.material;
            std::shared_ptr<Mesh> meshPtr = ProcessMesh(gltf, prim, materialIndex, modelNode.globalTransform);
            if (meshPtr) {
                int newMeshIndex = static_cast<int>(meshes.size());
                meshes.push_back(meshPtr);
                meshLocalTransforms.push_back(modelNode.globalTransform);
                meshNames.push_back(gltfMesh.name);
                // 仅设置第一个 Primitive 的 meshIndex
                if (modelNode.meshIndex < 0) { modelNode.meshIndex = newMeshIndex; }
            }
        }
    }

    // 3. 递归处理子节点
    int currNodeIndex = modelNodes.size();
    modelNodes.push_back(modelNode);
    for (int childIndex : node.children) {
        int childModelNodeIndex = ProcessNode(gltf, childIndex, modelNode.globalTransform);
        modelNodes[currNodeIndex].children.push_back(childModelNodeIndex);
    }
    return currNodeIndex;
}


std::shared_ptr<Mesh> Model::ProcessMesh(const tinygltf::Model& gltf, const tinygltf::Primitive& prim, int materialIndex, const glm::mat4& worldTransform)
{
    // ------------------------------
    // 1. Validate index accessor
    // ------------------------------
    if (prim.indices < 0) {
        std::cerr << "[Model] Primitive has no indices.\n";
        return nullptr;
    }

    const tinygltf::Accessor& indexAccessor = gltf.accessors[prim.indices];
    const tinygltf::BufferView& indexView = gltf.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = gltf.buffers[indexView.buffer];

    const unsigned char* indexData = indexBuffer.data.data()
        + indexView.byteOffset
        + indexAccessor.byteOffset;

    std::vector<unsigned int> indices;
    indices.reserve(indexAccessor.count);

    // ------------------------------
    // 2. Parse index buffer
    // ------------------------------
    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* buf = reinterpret_cast<const uint16_t*>(indexData);
        for (size_t i = 0; i < indexAccessor.count; i++)
            indices.push_back(static_cast<unsigned int>(buf[i]));
    }
    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const uint32_t* buf = reinterpret_cast<const uint32_t*>(indexData);
        for (size_t i = 0; i < indexAccessor.count; i++)
            indices.push_back(buf[i]);
    }
    else {
        std::cerr << "[Model] Unsupported index component type.\n";
        return nullptr;
    }

    // ------------------------------
    // 3. Determine vertex count
    // POSITION attribute is mandatory
    // ------------------------------
    if (prim.attributes.find("POSITION") == prim.attributes.end()) {
        std::cerr << "[Model] Primitive missing POSITION attribute.\n";
        return nullptr;
    }

    const int posAccessorIndex = prim.attributes.at("POSITION");
    const tinygltf::Accessor& posAccessor = gltf.accessors[posAccessorIndex];
    size_t vertexCount = posAccessor.count;

    std::vector<Vertex> vertices(vertexCount);

    // 初始化第二套纹理坐标为第一套的值
    for (size_t i = 0; i < vertexCount; i++) {
        vertices[i].TexCoords2 = glm::vec2(0.0f, 0.0f);
    }

    // ------------------------------
    // Helper lambda: Read attribute
    // ------------------------------
    auto ReadAttribute = [&](const char* attrName,
        std::function<void(size_t, const unsigned char*)> assignFn)
        {
            if (prim.attributes.find(attrName) == prim.attributes.end())
                return;

            const int accessorIndex = prim.attributes.at(attrName);
            const tinygltf::Accessor& accessor = gltf.accessors[accessorIndex];
            const tinygltf::BufferView& view = gltf.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = gltf.buffers[view.buffer];

            const unsigned char* dataPtr = buffer.data.data()
                + view.byteOffset
                + accessor.byteOffset;

            size_t stride = accessor.ByteStride(view);
            if (stride == 0) {
                std::cerr << "[Model] Invalid stride in attribute.\n";
                return;
            }

            for (size_t i = 0; i < accessor.count; i++)
                assignFn(i, dataPtr + stride * i);
        };

    // ------------------------------
    // 4. Read POSITION
    // ------------------------------
    ReadAttribute("POSITION", [&](size_t i, const unsigned char* ptr) {
        const float* f = reinterpret_cast<const float*>(ptr);
        vertices[i].Position = glm::vec3(f[0], f[1], f[2]);
        });

    // ------------------------------
    // 5. Read NORMAL (optional)
    // ------------------------------
    ReadAttribute("NORMAL", [&](size_t i, const unsigned char* ptr) {
        const float* f = reinterpret_cast<const float*>(ptr);
        vertices[i].Normal = glm::vec3(f[0], f[1], f[2]);
        });

    // ------------------------------
    // 6. Read TEXCOORD_0 (optional)
    // ------------------------------
    ReadAttribute("TEXCOORD_0", [&](size_t i, const unsigned char* ptr) {
        const float* f = reinterpret_cast<const float*>(ptr);
        vertices[i].TexCoords = glm::vec2(f[0], f[1]);
        if (vertices[i].TexCoords2.x == 0.0f && vertices[i].TexCoords2.y == 0.0f)
        {
            vertices[i].TexCoords2 = vertices[i].TexCoords;
        }
        });

    ReadAttribute("TEXCOORD_1", [&](size_t i, const unsigned char* ptr) {
        const float* f = reinterpret_cast<const float*>(ptr);
        // 如果存在TEXCOORD_1，则用它替换第二套纹理坐标
        vertices[i].TexCoords2 = glm::vec2(f[0], f[1]);
        // std::cout << "TexCoord 1: " << f[0] << ", " << f[1] << std::endl;
        });

    // ------------------------------
    // 7. Read TANGENT (optional, glTF uses vec4)
    // ------------------------------
    ReadAttribute("TANGENT", [&](size_t i, const unsigned char* ptr) {
        const float* f = reinterpret_cast<const float*>(ptr);
        vertices[i].Tangent = glm::vec3(f[0], f[1], f[2]);
        vertices[i].Handedness = f[3];
        // f[3] is handedness (ignored for now)
        });

    // 计算 bitangent
    for (size_t i = 0; i < vertexCount; i++) {
        vertices[i].Bitangent = vertices[i].Handedness * glm::cross(vertices[i].Normal, vertices[i].Tangent);

    }

    // ------------------------------
    // Accumulate local AABB and transform to world space for scene bounds
    // ------------------------------
    glm::vec3 localMin(FLT_MAX);
    glm::vec3 localMax(-FLT_MAX);
    for (const auto& v : vertices) {
        localMin = glm::min(localMin, v.Position);
        localMax = glm::max(localMax, v.Position);
    }
    glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMin.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMax.z},
        {localMax.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMax.z}
    };
    for (int i = 0; i < 8; i++) {
        glm::vec3 world = glm::vec3(worldTransform * glm::vec4(corners[i], 1.0f));
        sceneMin_ = glm::min(sceneMin_, world);
        sceneMax_ = glm::max(sceneMax_, world);
    }

    // ------------------------------
    // Construct final Mesh object
    // ------------------------------
    return std::make_shared<Mesh>(vertices, indices, materialIndex);
}


void Model::LoadMaterials(const tinygltf::Model& gltf) {
    materials.clear();
    materials.reserve(gltf.materials.size());

    // 通用读取函数：支持 TextureInfo / NormalTextureInfo / OcclusionTextureInfo / EmissiveTextureInfo / PbrMetallicRoughness::baseColorTexture / PbrMetallicRoughness::metallicRoughnessTexture
    auto ReadTextureInfo = [](const auto& src, PBRTextureInfo& dst) {
        dst.textureIndex = src.index;
        dst.texCoord = src.texCoord;
        dst.hasTransform = false;
        dst.offset = glm::vec2(0.0f);
        dst.scale = glm::vec2(1.0f);
        dst.rotation = 0.0f;

        auto it = src.extensions.find("KHR_texture_transform");
        if (it == src.extensions.end()) {
            return;
        }

        const tinygltf::Value& ext = it->second;
        dst.hasTransform = true;

        if (ext.Has("offset")) {
            const auto& arr = ext.Get("offset").Get<tinygltf::Value::Array>();
            if (arr.size() >= 2) {
                dst.offset.x = static_cast<float>(arr[0].GetNumberAsDouble());
                dst.offset.y = static_cast<float>(arr[1].GetNumberAsDouble());
            }
        }
        if (ext.Has("scale")) {
            const auto& arr = ext.Get("scale").Get<tinygltf::Value::Array>();
            if (arr.size() >= 2) {
                dst.scale.x = static_cast<float>(arr[0].GetNumberAsDouble());
                dst.scale.y = static_cast<float>(arr[1].GetNumberAsDouble());
            }
        }
        if (ext.Has("rotation")) {
            dst.rotation = static_cast<float>(
                ext.Get("rotation").GetNumberAsDouble());
        }
        if (ext.Has("texCoord")) {
            dst.texCoord = static_cast<int>(
                ext.Get("texCoord").GetNumberAsInt());
        }
        };

    for (const auto& m : gltf.materials) {
        PBRMaterial mat;
        mat.name = m.name;

        // baseColorFactor
        if (m.pbrMetallicRoughness.baseColorFactor.size() == 4) {
            mat.baseColorFactor = glm::vec4(
                static_cast<float>(m.pbrMetallicRoughness.baseColorFactor[0]),
                static_cast<float>(m.pbrMetallicRoughness.baseColorFactor[1]),
                static_cast<float>(m.pbrMetallicRoughness.baseColorFactor[2]),
                static_cast<float>(m.pbrMetallicRoughness.baseColorFactor[3]));
        }

        // metallic / roughness
        mat.metallicFactor = static_cast<float>(m.pbrMetallicRoughness.metallicFactor);
        mat.roughnessFactor = static_cast<float>(m.pbrMetallicRoughness.roughnessFactor);
        // transparency: alphaMode + alphaCutoff
        if (m.alphaMode == "BLEND") {
            mat.alphaMode = AlphaMode::BLEND_MODE;
        }
        else if (m.alphaMode == "MASK") {
            mat.alphaMode = AlphaMode::MASK_MODE;
            mat.alphaCutoff = static_cast<float>(m.alphaCutoff); // default: 0.5
        }
        else {
            mat.alphaMode = AlphaMode::OPAQUE_MODE;
        }

        // 读取所有纹理：包含 KHR_texture_transform
        ReadTextureInfo(m.pbrMetallicRoughness.baseColorTexture, mat.baseColorTexture);
        ReadTextureInfo(m.pbrMetallicRoughness.metallicRoughnessTexture, mat.metallicRoughnessTexture);
        ReadTextureInfo(m.normalTexture, mat.normalTexture);
        ReadTextureInfo(m.occlusionTexture, mat.occlusionTexture);
        ReadTextureInfo(m.emissiveTexture, mat.emissiveTexture);

        // emissiveFactor
        if (m.emissiveFactor.size() == 3) {
            mat.emissiveFactor = glm::vec3(
                static_cast<float>(m.emissiveFactor[0]),
                static_cast<float>(m.emissiveFactor[1]),
                static_cast<float>(m.emissiveFactor[2]));
        }

        materials.push_back(mat);
    }

    // // 添加材质信息输出用于调试
    // std::cout << "Loaded " << materials.size() << " materials:" << std::endl;
    // for (size_t i = 0; i < materials.size(); ++i) {
    //     const PBRMaterial& mat = materials[i];
    //     std::cout << "  Material #" << i << ": " << mat.name << std::endl;
    //     std::cout << "    Base Color: ("
    //         << mat.baseColorFactor.r << ", "
    //         << mat.baseColorFactor.g << ", "
    //         << mat.baseColorFactor.b << ", "
    //         << mat.baseColorFactor.a << ")" << std::endl;
    //     std::cout << "    Metallic: " << mat.metallicFactor
    //         << ", Roughness: " << mat.roughnessFactor << std::endl;

    //     // 输出纹理信息
    //     if (mat.baseColorTexture.hasTexture()) {
    //         std::cout << "    Base Color Texture: index=" << mat.baseColorTexture.textureIndex
    //             << ", texCoord=" << mat.baseColorTexture.texCoord;
    //         if (mat.baseColorTexture.hasTransform) {
    //             std::cout << ", transform(offset=(" << mat.baseColorTexture.offset.x << "," << mat.baseColorTexture.offset.y
    //                 << "), scale=(" << mat.baseColorTexture.scale.x << "," << mat.baseColorTexture.scale.y
    //                 << "), rotation=" << mat.baseColorTexture.rotation << ")";
    //         }
    //         std::cout << std::endl;
    //     }
    //     else {
    //         std::cout << "    Base Color Texture: None" << std::endl;
    //     }

    //     if (mat.metallicRoughnessTexture.hasTexture()) {
    //         std::cout << "    Metallic-Roughness Texture: index=" << mat.metallicRoughnessTexture.textureIndex
    //             << ", texCoord=" << mat.metallicRoughnessTexture.texCoord;
    //         if (mat.metallicRoughnessTexture.hasTransform) {
    //             std::cout << ", transform(offset=(" << mat.metallicRoughnessTexture.offset.x << "," << mat.metallicRoughnessTexture.offset.y
    //                 << "), scale=(" << mat.metallicRoughnessTexture.scale.x << "," << mat.metallicRoughnessTexture.scale.y
    //                 << "), rotation=" << mat.metallicRoughnessTexture.rotation << ")";
    //         }
    //         std::cout << std::endl;
    //     }
    //     else {
    //         std::cout << "    Metallic-Roughness Texture: None" << std::endl;
    //     }

    //     if (mat.normalTexture.hasTexture()) {
    //         std::cout << "    Normal Texture: index=" << mat.normalTexture.textureIndex
    //             << ", texCoord=" << mat.normalTexture.texCoord;
    //         if (mat.normalTexture.hasTransform) {
    //             std::cout << ", transform(offset=(" << mat.normalTexture.offset.x << "," << mat.normalTexture.offset.y
    //                 << "), scale=(" << mat.normalTexture.scale.x << "," << mat.normalTexture.scale.y
    //                 << "), rotation=" << mat.normalTexture.rotation << ")";
    //         }
    //         std::cout << std::endl;
    //     }
    //     else {
    //         std::cout << "    Normal Texture: None" << std::endl;
    //     }
    // }
}


void Model::LoadTextures(const tinygltf::Model& gltf) {
    textures.resize(gltf.textures.size());

    for (size_t i = 0; i < gltf.textures.size(); i++) {
        const tinygltf::Texture& tex = gltf.textures[i];

        int src = tex.source; // index into gltf.images
        if (src < 0 || src >= gltf.images.size())
            continue;

        const tinygltf::Image& img = gltf.images[src];

        textures[i].LoadFromMemory(
            img.image.data(),
            img.width,
            img.height,
            img.component,
            true
        );
    }

    // 添加纹理信息输出用于调试
    // std::cout << "Loaded " << textures.size() << " textures:" << std::endl;
    // for (size_t i = 0; i < gltf.textures.size(); i++) {
    //     const tinygltf::Texture& tex = gltf.textures[i];
    //     int src = tex.source;
    //     if (src < 0 || src >= gltf.images.size()) {
    //         std::cout << "  Texture #" << i << ": Invalid source image" << std::endl;
    //         continue;
    //     }

    //     const tinygltf::Image& img = gltf.images[src];
    //     std::cout << "  Texture #" << i << ": " << img.width << "x" << img.height
    //         << " pixels, " << img.component << " components" << std::endl;
    // }
}


void Model::LoadCameras(const tinygltf::Model& gltf) {
    cameras.clear();
    cameras.reserve(gltf.cameras.size());
    for (size_t i = 0; i < gltf.nodes.size(); i++) {
        const auto& node = gltf.nodes[i];
        if (node.camera < 0) continue;
        const auto& gltfCam = gltf.cameras[node.camera];
        CamInfos info;
        info.POSITION = glm::vec3(
            modelNodes[i].globalTransform[3][0],
            modelNodes[i].globalTransform[3][1],
            modelNodes[i].globalTransform[3][2]
        );

        // 从变换矩阵提取相机朝向
        glm::mat4 transform = modelNodes[i].globalTransform;
        glm::vec3 forward = -glm::normalize(glm::vec3(transform[2])); // glTF相机默认朝向-Z

        // 计算YAW (绕Y轴旋转)
        info.YAW = glm::degrees(atan2(forward.z, forward.x));

        // 计算PITCH (绕X轴旋转)
        info.PITCH = glm::degrees(asin(forward.y));

        if (gltfCam.type == "perspective") {
            info.FOV = glm::degrees((float)gltfCam.perspective.yfov);
            info.NEAR_PLANE = (float)gltfCam.perspective.znear;
            info.FAR_PLANE = (float)gltfCam.perspective.zfar;
        }
        cameras.push_back(info);
    }
}

void Model::LoadLights(const tinygltf::Model& gltf) {
    lights_.clear();

    // 遍历所有节点，查找包含光源扩展的节点
    for (int i = 0; i < modelNodes.size(); ++i) {
        ProcessLightNode(gltf, i, modelNodes[i].globalTransform);
    }

    // std::cout << "Loaded " << lights_.size() << " lights from glTF model" << std::endl;
}

void Model::ProcessLightNode(const tinygltf::Model& gltf, int nodeIndex, const glm::mat4& transform) {
    if (nodeIndex >= gltf.nodes.size()) return;

    const auto& node = gltf.nodes[nodeIndex];

    // 检查节点是否有KHR_lights_punctual扩展
    auto extensionsIt = node.extensions.find("KHR_lights_punctual");
    if (extensionsIt != node.extensions.end()) {
        const auto& lightExt = extensionsIt->second;

        // 解析光源索引
        if (lightExt.Has("light") && lightExt.Get("light").IsNumber()) {
            int lightIndex = lightExt.Get("light").GetNumberAsInt();

            // 检查glTF模型是否有光源扩展
            auto gltfExtIt = gltf.extensions.find("KHR_lights_punctual");
            if (gltfExtIt != gltf.extensions.end()) {
                const auto& lightsArray = gltfExtIt->second.Get("lights");
                if (lightsArray.IsArray() && lightIndex >= 0 && lightIndex < lightsArray.ArrayLen()) {
                    const auto& lightData = lightsArray.Get(lightIndex);

                    Light light;

                    // 解析光源类型
                    if (lightData.Has("type")) {
                        std::string type = lightData.Get("type").Get<std::string>();
                        if (type == "directional") {
                            light.type = LightType::DIRECTIONAL;
                        }
                        else if (type == "point") {
                            light.type = LightType::POINT;
                        }
                        else if (type == "spot") {
                            light.type = LightType::SPOT;
                        }
                    }

                    // 解析颜色
                    if (lightData.Has("color")) {
                        const auto& color = lightData.Get("color");
                        if (color.IsArray() && color.ArrayLen() >= 3) {
                            light.color = glm::vec3(
                                color.Get(0).GetNumberAsDouble(),
                                color.Get(1).GetNumberAsDouble(),
                                color.Get(2).GetNumberAsDouble()
                            );
                        }
                    }

                    // 解析强度
                    if (lightData.Has("intensity")) {
                        light.intensity = lightData.Get("intensity").GetNumberAsDouble();
                    }

                    // 从变换矩阵提取位置和方向
                    light.position = glm::vec3(transform[3]);
                    light.direction = glm::normalize(glm::vec3(transform * glm::vec4(0, 0, -1, 0)));

                    // 解析聚光灯参数（如果是聚光灯）
                    if (light.type == LightType::SPOT && lightData.Has("spot")) {
                        const auto& spot = lightData.Get("spot");
                        if (spot.Has("innerConeAngle")) {
                            light.innerConeAngle = spot.Get("innerConeAngle").GetNumberAsDouble();
                        }
                        if (spot.Has("outerConeAngle")) {
                            light.outerConeAngle = spot.Get("outerConeAngle").GetNumberAsDouble();
                        }
                    }

                    lights_.push_back(light);
                    std::cout << "Found light: type=" << static_cast<int>(light.type) << " intensity=" << light.intensity << std::endl;
                }
            }
        }
    }
}