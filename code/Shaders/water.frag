#version 410 core

in vec3 WorldPos;
in vec3 Normal;
in vec4 clipSpacePosReal;

in vec2 TexCoords;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vNormal;
in vec4 vLightSpacePos;

out vec4 FragColor;

// Textures
uniform sampler2D reflectionTex;
uniform sampler2D refractionTex;
uniform sampler2D refractionDepthTex;
// Distortion maps
uniform sampler2D dudvMap;
// Time and wave parameters
uniform float time;
uniform float dudvOffset;
uniform float waveStrength;
uniform float normalRippleStrength;  // 控制法线micro-ripples强度
uniform vec2 waveDirection1;         // 波浪方向 1
uniform vec2 waveDirection2;         // 波浪方向 2
uniform float waveScale1;            // 波浪缩放 1
uniform float waveScale2;            // 波浪缩放 2
// Water parameters
uniform float fresnelPower;
uniform float deepBoost;
uniform float eta;               // 水面折射率
uniform vec3 absorptionCoeff;    //Beer-Lambert 吸收系数
uniform vec3 inScatteringColor;  //水体自身散射颜色

// Camera matrices and position
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec2 nearFarPlanes;

float LinearizeDepth(float depth, vec2 nearFar)
{
    float z = depth * 2.0 - 1.0;  // Back to NDC
    return (2.0 * nearFar.x * nearFar.y) / (nearFar.y + nearFar.x - z * (nearFar.y - nearFar.x));
}

// 简化的波浪扰动函数
vec3 SimpleWaveNormal(vec2 uv, vec2 direction, float scale, float speed)
{
    vec2 d = normalize(direction);
    float wave = sin(dot(d, uv) * scale - time * speed) * 0.1;
    return vec3(d.x * wave, 0.0, d.y * wave);
}

// 简化的水面法线计算（不使用法线贴图）
vec3 CalculateWaterNormal(vec2 uv, vec3 baseNormal)
{
    // 1. 简化的波浪效果
    vec3 wave1 = SimpleWaveNormal(uv * waveScale1, waveDirection1, 4.0, 0.8);
    vec3 wave2 = SimpleWaveNormal(uv * waveScale2, waveDirection2, 6.0, 1.2);
    vec3 waveNormal = (wave1 + wave2) * 0.15;

    // 2. DUDV微波纹（保持细节）
    vec2 dudvSample1 = texture(dudvMap, uv + vec2(time * 0.03, time * 0.02)).rg;
    vec2 dudvSample2 = texture(dudvMap, uv * 1.2 - vec2(time * 0.015, -time * 0.025)).rg;
    vec2 dudv = (dudvSample1 + dudvSample2) * 0.5;
    dudv = dudv * 2.0 - 1.0;

    vec3 dudvNormal = vec3(dudv.x, 0.0, dudv.y) * normalRippleStrength;

    // 组合波浪和DUDV效果
    return normalize(baseNormal + waveNormal + dudvNormal);
}

void main()
{
    //* 改进的水面法线和扰动计算
    // ------------------------------
    vec3 V = normalize(cameraPos - WorldPos);
    vec3 I = -V;

    // 真实相机 NDC
    vec2 ndcReal = clipSpacePosReal.xy / clipSpacePosReal.w;
    vec2 uvReal = ndcReal * 0.5 + 0.5;

    // 使用改进的法线计算
    vec3 baseNormal = normalize(vNormal);
    vec3 N = CalculateWaterNormal(uvReal, baseNormal);

    // 重新计算DUDV扰动，使其与法线更好地协同
    vec2 dudvSample1 = texture(dudvMap, uvReal + vec2(time * 0.05, time * 0.03)).rg;
    vec2 dudvSample2 = texture(dudvMap, uvReal * 1.35 - vec2(time * 0.02, -time * 0.04)).rg;
    vec2 dudv = (dudvSample1 + dudvSample2) * 0.5;
    dudv = dudv * 2.0 - 1.0;

    // Scale DUDV for UV distortion (separate control)
    dudv *= waveStrength;
    // 用物理折射方向调节折射扰动强度
    vec3 refractedDir = refract(I, N, 1.0 / eta);
    float bendFactor = clamp(length(refractedDir.xy / max(1e-3, refractedDir.z)), 0.0, 1.5);
    vec2 uvRealDistorted = clamp(uvReal + dudv * bendFactor, 0.001, 0.999);

    //* 自然的Fresnel计算（liminal pool风格）
    // ------------------------------
    float cosTheta = clamp(dot(N, V), 0.0, 1.0);
    float F0 = pow((eta - 1.0) / (eta + 1.0), 2.0);

    // 简化的Fresnel，减少镜面反射
    float fresnelFactor = F0 + (1.0 - F0) * pow(1.0 - cosTheta, fresnelPower);
    fresnelFactor = clamp(fresnelFactor, 0.1, 0.8);  // 限制反射范围，减少镜面感

    //* 从FBO采样反射和折射纹理
    // ------------------------------
    vec3 reflectionColor =
        texture(reflectionTex, vec2(uvRealDistorted.x, 1.0 - uvRealDistorted.y)).rgb;
    vec3 refractionColor = texture(refractionTex, uvRealDistorted).rgb;

    //* 水深计算与颜色混合
    // ------------------------------
    float depthValue = texture(refractionDepthTex, uvRealDistorted).r;
    float sceneDepthVS =
        LinearizeDepth(depthValue, nearFarPlanes);  //水下场景深度（视空间线性深度）
    vec4 viewPosWater = view * vec4(WorldPos, 1.0);
    float waterDepthVS = -viewPosWater.z;  //水面深度（视空间线性深度,相机看向 -Z 方向）

    float thickness = sceneDepthVS - waterDepthVS;  //水体厚度（视空间线性深度）
    thickness = max(thickness, 0.0);

    // Beer-Lambert：按通道计算透射率 T(d) = exp(-σ * d)
    vec3 transmittance = exp(-absorptionCoeff * thickness * deepBoost);
    vec3 finalRefraction =
        refractionColor * transmittance + inScatteringColor * (1.0 - transmittance);

    //* 最终颜色混合
    // ------------------------------
    vec3 finalColor = mix(finalRefraction, reflectionColor, fresnelFactor);

    // 稳定的透明度，不随波浪变化
    FragColor = vec4(finalColor, 0.82);
}