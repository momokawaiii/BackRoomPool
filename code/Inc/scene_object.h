#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "irenderable.h"
#include "model.h"
#include "shader.h"

// 前向声明
class ShadowSystem;
class LightingSystem;


//* SceneObject 是场景中的一系列 Mesh 实例的集合体,这些 Mesh实例共享了同一个 Shader 资源(纹理、材质等)
//* SceneObject 构成了一个独立的可渲染实体，拥有自己的变换属性（位置、旋转、缩放）
//* 场景中的每个物体(建筑、金属梯、游泳圈、水面)都可以表示为一个 SceneObject 对象


//* 你的任务是在 SceneObject 类的基础上派生出各个具体的场景物体类，* 例如 Building、Ladder、SwimmingRing、WaterSurface 等等
//* 每个派生类可以根据需要添加特定的属性和方法,还需要添加对应的着色器文件(顶点着色器和片段着色器)
//* 下面的示例展示了在不区分具体 Mesh 的情况下，如何对整个物体进行渲染

class SceneObject : public IRenderable {
public:
    //Model 负责加载blender导出的 glTF 文件,其中存储了所有Mesh的几何信息和 材质信息(可能有?)
    //Model 作为资源库, 可以被多个 SceneObject 实例共享
    //ScneObject 则负责具体的渲染和变换操作,它在Model中收集所属的 Mesh 实例指针
    //rootNode 参数指定了场景图中的根节点,SceneObject 将渲染该节点及其所有子节点包含的 Mesh
    SceneObject(const Model* model, int rootNodeIndex);

    // IRenderable 是一个接口类, 定义了 Init/Update/Render 三个纯虚函数
    // 它表示一个可渲染的实体, 需要在渲染循环中被初始化、更新和渲染
    // Init 函数负责加载和编译 Shader 等渲染资源
    // Update 函数负责更新每帧的状态(例如动画)
    // Render 函数负责实际的绘制调用,你需要调用 Shader 和 Mesh 的绘制方法,为此你应当了解着色器编程 GLSL 
    void Init() override;
    void Update(float dt) override;
    void Render(const Camera& camera, float aspect) override;

    //其它的渲染方法接口
    void RenderWithClip(const Camera& camera, float aspect, const glm::vec4& clipPlane);

    // 阴影深度渲染（从光源视角渲染几何体到深度缓冲）
    void RenderShadowDepth();

    // 设置光照系统
    void SetLightingSystem(const class LightingSystem* lightingSystem) { lightingSystem_ = lightingSystem; }

    // 设置阴影系统
    void SetShadowSystem(const class ShadowSystem* shadowSystem) { shadowSystem_ = shadowSystem; }

    // ===== 变换接口 =====
    void SetPosition(const glm::vec3& p);
    void SetRotationEuler(const glm::vec3& eulerDegrees);
    void SetScale(const glm::vec3& s);

    const glm::vec3& GetPosition() const { return position; }
    const glm::vec3& GetRotationEuler() const { return rotationEulerDeg; }
    const glm::vec3& GetScale() const { return scale; }
    glm::mat4 GetModelMatrix() const;

protected:
    // ===== 渲染资源 =====
    const Model* model;  ///< 所属模型资源
    int rootNodeIndex;   ///< 场景图中的根节点索引
    std::unique_ptr<Shader> shader;  ///< 渲染该物体的着色器
    std::unique_ptr<Shader> shadowShader;  ///< 阴影深度渲染着色器
    const class LightingSystem* lightingSystem_ = nullptr;  ///< 光照系统指针
    const class ShadowSystem* shadowSystem_ = nullptr;  ///< 阴影系统指针

    // ===== 变换状态 =====
    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 rotationEulerDeg{ 0.0f, 0.0f, 0.0f };  ///< XYZ 欧拉角（单位：度）
    glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

    // ===== 内部工具函数 =====
    glm::mat4 ComputeModelMatrix() const;
};