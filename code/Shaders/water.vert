#version 410 core

// 顶点属性: 位置, 法线, UV坐标
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

// MVP变换矩阵
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// 裁剪平面(世界空间)
// Ax + By + Cz + D = 0
uniform vec4 clipPlane;

// 阴影相关
uniform mat4 lightSpaceMatrix;

// 输出到frag着色器的插值
out vec3 WorldPos;
out vec3 Normal;
out vec4 clipSpacePosReal;
out vec2 TexCoords;

out vec3 vTangent;
out vec3 vBitangent;
out vec3 vNormal;
out vec4 vLightSpacePos;

void main()
{
    // 把顶点从模型空间转换到世界空间
    vec4 world = model * vec4(aPos, 1.0);
    WorldPos = world.xyz;

    // 法线变换
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // 齐次世界坐标系代入平面方程进行裁剪
    // gl_ClipDistance[0] < 0 时裁剪该顶点
    gl_ClipDistance[0] = dot(vec4(WorldPos, 1.0), clipPlane);

    TexCoords = aTexCoord;

    // Build world‑space TBN
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vNormal = normalize(normalMatrix * aNormal);
    vTangent = normalize(normalMatrix * aTangent);
    vBitangent = normalize(normalMatrix * aBitangent);

    // 计算光空间位置用于阴影计算
    vLightSpacePos = lightSpaceMatrix * world;

    // 最终顶点位置
    // ⚠️ 分别计算真实相机与反射相机的 clip space 位置
    clipSpacePosReal = projection * view * world;
    // 最终使用真实相机的视角绘制水面几何
    gl_Position = clipSpacePosReal;
}