#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in vec2 aUV2;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;

uniform vec4 clipPlane;

// 阴影相关
uniform mat4 lightSpaceMatrix;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec2 vUV2;
out vec3 vTangent;
out vec3 vBitangent;
out vec4 vLightSpacePos;

void main()
{
    // 模型空间->世界空间
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vUV = aUV;
    vUV2 = aUV2;

    // 传递切线和副切线到片段着色器
    vTangent = mat3(transpose(inverse(model))) * aTangent;
    vBitangent = mat3(transpose(inverse(model))) * aBitangent;

    // 计算光空间位置用于阴影计算
    vLightSpacePos = lightSpaceMatrix * worldPos;

    gl_ClipDistance[0] = dot(worldPos, clipPlane);
    gl_Position = projection * view * worldPos;
}