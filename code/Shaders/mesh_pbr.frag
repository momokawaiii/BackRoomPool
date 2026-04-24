#version 410 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec2 vUV2;
in vec3 vTangent;
in vec3 vBitangent;
in vec4 vLightSpacePos;

out vec4 FragColor;

// 光源结构体
struct Light
{
    int type;  // 0=directional, 1=point, 2=spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float constant;  // 点光源衰减
    float linear;
    float quadratic;
    float innerConeAngle;  // 聚光灯内锥角
    float outerConeAngle;  // 聚光灯外锥角
};

#define MAX_LIGHTS 32
uniform Light lights[MAX_LIGHTS];
uniform int numLights;
uniform vec3 ambientLight;
uniform vec3 cameraPos;

// 材质参数
uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float occlusionStrength;

uniform sampler2D baseColorTex;
uniform sampler2D metalRoughTex;
uniform sampler2D normalTex;
uniform sampler2D occlusionTex;

uniform int hasBaseColorTex;
uniform int hasMetalRoughTex;
uniform int hasNormalTex;
uniform int hasOcclusionTex;

// 纹理坐标集合索引
uniform int baseColor_texCoordSet;
uniform int metalRough_texCoordSet;
uniform int normal_texCoordSet;
uniform int occlusion_texCoordSet;

// KHR_texture_transform 变换
uniform int baseColor_hasTransform;
uniform vec2 baseColor_offset;
uniform vec2 baseColor_scale;
uniform float baseColor_rotation;

uniform int metalRough_hasTransform;
uniform vec2 metalRough_offset;
uniform vec2 metalRough_scale;
uniform float metalRough_rotation;

uniform int normal_hasTransform;
uniform vec2 normal_offset;
uniform vec2 normal_scale;
uniform float normal_rotation;

uniform int occlusion_hasTransform;
uniform vec2 occlusion_offset;
uniform vec2 occlusion_scale;
uniform float occlusion_rotation;

// IBL纹理
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform int hasIBL;

// IBL强度控制
uniform float iblIntensity;
uniform float diffuseIBLIntensity;
uniform float specularIBLIntensity;

// 阴影相关
uniform int hasShadow;
uniform sampler2D shadowMap;
uniform float shadowBias;
uniform float shadowIntensity;

// 纹理变换函数
vec2 ApplyTexTransform(vec2 uv, int hasTransform, vec2 offset, vec2 scale, float rotation)
{
    if (hasTransform == 0)
        return uv;

    float s = sin(rotation);
    float c = cos(rotation);
    mat2 R = mat2(c, -s, s, c);

    vec2 t = uv;
    t = R * t;
    t *= scale;
    t += offset;

    return t;
}

// 根据纹理坐标集索引选择纹理坐标
vec2 GetTexCoord(int texCoordSet)
{
    if (texCoordSet == 0)
        return vUV;
    else
        return vUV2;
}

// PBR 函数
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.1415926 * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 计算点光源衰减
float CalculateAttenuation(Light light, vec3 worldPos)
{
    if (light.type == 0)  // directional light
        return 1.0;

    float distance = length(light.position - worldPos);
    return 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
}

// 计算聚光灯衰减
float CalculateSpotlightAttenuation(Light light, vec3 worldPos)
{
    if (light.type != 2)  // not a spotlight
        return 1.0;

    vec3 lightToFragment = normalize(worldPos - light.position);
    float cosTheta = dot(lightToFragment, normalize(light.direction));
    float epsilon = cos(light.innerConeAngle) - cos(light.outerConeAngle);
    float intensity = clamp((cosTheta - cos(light.outerConeAngle)) / epsilon, 0.0, 1.0);
    return intensity;
}

// 计算单个光源的贡献（考虑阴影遮挡）
vec3 CalculateLightContribution(Light light,
                                vec3 N,
                                vec3 V,
                                vec3 worldPos,
                                vec3 albedo,
                                float metallic,
                                float roughness,
                                vec3 F0,
                                float shadowFactor)
{
    vec3 L;
    if (light.type == 0)  // directional light
        L = normalize(-light.direction);
    else  // point or spot light
        L = normalize(light.position - worldPos);

    vec3 H = normalize(V + L);

    // 计算衰减
    float attenuation = CalculateAttenuation(light, worldPos);
    attenuation *= CalculateSpotlightAttenuation(light, worldPos);

    vec3 radiance = light.color * light.intensity * attenuation;

    // PBR计算
    float NdotL = max(dot(N, L), 0.0);

    // Cook-Torrance BRDF
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    // 将阴影因子应用到整个光照贡献（漫反射 + 镜面反射）
    return (kD * albedo / 3.1415926 + specular) * radiance * NdotL * shadowFactor;
}

// 阴影计算函数（带PCF软阴影）
float CalculateShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    if (hasShadow == 0)
    {
        return 1.0;  // 无阴影，完全照亮
    }

    // 透视除法
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // 转换到[0,1]范围
    projCoords = projCoords * 0.5 + 0.5;

    // 严格的边界检查 - 如果超出阴影贴图范围，视为无阴影
    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 1.0;
    }

    // 当前片段的深度
    float currentDepth = projCoords.z;

    // 自适应偏移计算
    float cosTheta = dot(normal, normalize(-lightDir));
    float bias = max(shadowBias * (1.0 - cosTheta), shadowBias * 0.1);

    // 防止偏移过大导致光线泄漏
    bias = min(bias, 0.01);

    // PCF (Percentage Closer Filtering) 软阴影
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    int pcfRadius = 1;  // 采样半径

    for (int x = -pcfRadius; x <= pcfRadius; ++x)
    {
        for (int y = -pcfRadius; y <= pcfRadius; ++y)
        {
            vec2 sampleCoord = projCoords.xy + vec2(x, y) * texelSize;

            // 确保采样坐标在有效范围内
            if (sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 && sampleCoord.y >= 0.0 &&
                sampleCoord.y <= 1.0)
            {
                float pcfDepth = texture(shadowMap, sampleCoord).r;
                shadow += (currentDepth - bias) > pcfDepth ? shadowIntensity : 0.0;
            }
        }
    }

    shadow /= float((2 * pcfRadius + 1) * (2 * pcfRadius + 1));

    return 1.0 - shadow;
}

void main()
{
    // 构建TBN矩阵
    vec3 T = normalize(vTangent);
    vec3 B = normalize(vBitangent);
    vec3 N = normalize(vNormal);
    mat3 TBN = mat3(T, B, N);

    // 法线贴图
    vec3 normalFromMap = vec3(0.0, 0.0, 1.0);
    if (hasNormalTex == 1)
    {
        vec2 uvNormal = ApplyTexTransform(GetTexCoord(normal_texCoordSet),
                                          normal_hasTransform,
                                          normal_offset,
                                          normal_scale,
                                          normal_rotation);
        normalFromMap = texture(normalTex, uvNormal).rgb;
        normalFromMap = normalize(normalFromMap * 2.0 - 1.0);
    }

    vec3 worldNormal = normalize(TBN * normalFromMap);

    // // 直接修复：如果法线与主光源夹角过大，调整法线方向
    // if (numLights > 0 && lights[0].type == 0)
    // {
    //     vec3 L = normalize(-lights[0].direction);
    //     if (dot(worldNormal, L) < 0.0)
    //     {
    //         // 如果dot product是负值，翻转法线
    //         worldNormal = -worldNormal;
    //     }
    // }
    vec3 V = normalize(cameraPos - vWorldPos);

    // 材质参数
    vec3 baseColor = baseColorFactor.rgb;
    vec2 uvBase = ApplyTexTransform(GetTexCoord(baseColor_texCoordSet),
                                    baseColor_hasTransform,
                                    baseColor_offset,
                                    baseColor_scale,
                                    baseColor_rotation);
    if (hasBaseColorTex == 1)
    {
        baseColor *= texture(baseColorTex, uvBase).rgb;
    }

    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    vec2 uvMR = ApplyTexTransform(GetTexCoord(metalRough_texCoordSet),
                                  metalRough_hasTransform,
                                  metalRough_offset,
                                  metalRough_scale,
                                  metalRough_rotation);
    if (hasMetalRoughTex == 1)
    {
        vec3 mr = texture(metalRoughTex, uvMR).rgb;
        metallic *= mr.b;
        roughness *= mr.g;
    }

    // 遮挡贴图采样
    float occlusion = 1.0;
    if (hasOcclusionTex == 1)
    {
        vec2 uvOcclusion = ApplyTexTransform(GetTexCoord(occlusion_texCoordSet),
                                             occlusion_hasTransform,
                                             occlusion_offset,
                                             occlusion_scale,
                                             occlusion_rotation);
        float occlusionValue = texture(occlusionTex, uvOcclusion).r;
        // 应用遮挡强度因子，线性插值以控制遮挡效果强度
        occlusion = mix(1.0, occlusionValue, occlusionStrength);
    }

    // F0 for dielectrics and metals
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    // 计算阴影衰减（仅对主方向光）
    float shadowFactor = 1.0;
    if (numLights > 0 && lights[0].type == 0)
    {
        // 对第一个方向光计算阴影
        shadowFactor = CalculateShadow(vLightSpacePos, worldNormal, lights[0].direction);
    }

    // 计算所有光源的贡献
    vec3 Lo = vec3(0.0);

    if (numLights > 0)
    {
        // 使用新的光照系统
        for (int i = 0; i < min(numLights, MAX_LIGHTS); ++i)
        {
            // 为每个光源确定阴影因子
            float currentShadowFactor = 1.0;

            // 仅对主方向光应用阴影映射
            if (i == 0 && lights[i].type == 0)
            {
                currentShadowFactor = shadowFactor;
            }
            // 其他光源目前不受阴影影响（可以后续扩展多光源阴影）

            vec3 lightContrib = CalculateLightContribution(lights[i],
                                                           worldNormal,
                                                           V,
                                                           vWorldPos,
                                                           baseColor,
                                                           metallic,
                                                           roughness,
                                                           F0,
                                                           currentShadowFactor);

            Lo += lightContrib;
        }
    }

    // 环境光照 (IBL + 基础环境光)
    vec3 ambient = ambientLight * baseColor * occlusion;

    if (hasIBL == 1)
    {
        // IBL 计算
        vec3 F = FresnelSchlickRoughness(max(dot(worldNormal, V), 0.0), F0, roughness);

        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        // 漫反射辐照度
        vec3 irradiance = texture(irradianceMap, worldNormal).rgb;
        vec3 diffuse = irradiance * baseColor * diffuseIBLIntensity * occlusion;

        // 镜面反射预滤波
        vec3 R = reflect(-V, worldNormal);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;

        // BRDF积分查找
        vec2 brdf = texture(brdfLUT, vec2(max(dot(worldNormal, V), 0.0), roughness)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y) * specularIBLIntensity;

        ambient += (kD * diffuse + specular) * iblIntensity;
    }

    vec3 color = Lo + ambient;

    // // HDR色调映射和gamma校正
    // color = color / (color + vec3(1.0));
    // color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}