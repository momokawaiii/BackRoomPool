#pragma once
#include <glm/glm.hpp>
#include "texture2d.h"

struct PBRTextureInfo {
    int textureIndex = -1;  // Model::textures[] index
    bool hasTexture() const { return textureIndex >= 0; }

    int texCoord = 0;        // TEXCOORD_0 / TEXCOORD_1
    bool hasTransform = false;
    glm::vec2 offset = glm::vec2(0.0f);
    glm::vec2 scale = glm::vec2(1.0f);
    float rotation = 0.0f;
};

enum class AlphaMode { OPAQUE_MODE, MASK_MODE, BLEND_MODE };

struct PBRMaterial {
    std::string name;

    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    PBRTextureInfo baseColorTexture;

    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    PBRTextureInfo metallicRoughnessTexture;

    PBRTextureInfo normalTexture;
    PBRTextureInfo occlusionTexture;
    float occlusionStrength = 1.0f;  // 遮挡强度因子 (0.0 = 无遮挡, 1.0 = 完全遮挡)
    PBRTextureInfo emissiveTexture;

    glm::vec3 emissiveFactor = glm::vec3(0.0f);

    AlphaMode alphaMode = AlphaMode::OPAQUE_MODE;
    float alphaCutoff = 0.5f; // Alpha cutoff for MASK mode
    // 将来可扩展：KHR extension
};