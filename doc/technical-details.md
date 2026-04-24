# PurePool 技术细节

本文面向希望理解 **PurePool** 代码结构与渲染实现细节的读者.我们以渲染数据流为主线，说明各模块为何如此组织、运行时如何协作，并标注关键着色器与渲染技术在工程中的具体实现位置.

> 约定：本文中提到的路径均相对于仓库根目录.默认配置下建议从 `build/` 目录启动程序，以保证以 `../` 为前缀的资源相对路径能够正确解析（见“构建与运行”）.若需在其他目录运行，可改为基于可执行文件目录定位资源或引入安装/打包阶段.

---

## 构建与运行（决定资源是否能加载）

项目通过 CMake 组织（见 `CMakeLists.txt`），第三方依赖以子模块/源码方式内置在 `external/`.

- 构建（推荐 out-of-source）：

```bash
mkdir -p build && cd build
cmake .. && cmake --build .
```

- 运行（默认配置下**建议**在 `build/` 内运行，否则以 `../` 引用的相对路径将无法解析）：

```bash
./PurePool
```

关键相对路径（均在运行时由代码直接引用）：
- glTF 模型：`../resources/blender/pools.glb`（`code/Src/renderer.cpp`）
- GLSL：`../code/Shaders/*`（`code/Src/scene_object.cpp`、`code/Src/water_surface.cpp`、`code/Src/skybox.cpp`、`code/Src/lighting_system.cpp`）
- HDR 环境贴图：`../resources/hdr/fog-moon_2K.hdr`（`code/Src/renderer.cpp`）

---

## 运行时主流程：从 App 到 Renderer

程序入口在 `code/main.cpp`：创建 `App`，随后进入 `App::run()` 的渲染循环（`code/Src/app.cpp`）.

### 事件与相机控制

`App` 负责 GLFW 窗口与输入回调：
- 键盘：W/A/S/D + Q/E → 调用 `Renderer::GetCamera().ProcessKeyboard(...)`
- 鼠标：`mouseCallback` 更新 yaw/pitch（FPS 视角），滚轮改变 FOV

窗口初始化过程中启用深度测试：
- `glEnable(GL_DEPTH_TEST)`（`code/Src/app.cpp`）

### Renderer 作为“渲染编排器”

`Renderer` 是管线的核心组织者（`code/Inc/renderer.h`、`code/Src/renderer.cpp`），负责：
- 持有并初始化 `Model`/`Camera`
- 初始化并更新 `LightingSystem`、`ShadowSystem`
- 维护可渲染模块列表：`SceneObject`（静态 PBR 物体）、`WaterSurface`（水面）、`Skybox`
- 管理反射/折射的 `Framebuffer`

渲染调用从 `App::render()` 进入 `Renderer::Render()`，其内部采用固定的 multi-pass 顺序（见后文“反射/折射与水面渲染”）.

---

## 资源加载：Model 与 glTF 2.0 的工程化落点

### glTF 2.0 的“最小理解”

在该工程中，glTF 的主要价值是把“场景图 + 网格 + 材质/纹理 + 相机/灯光”等数据打包成统一资产.
- **节点（Node）**：携带 TRS（平移/旋转/缩放）与子节点索引，组成场景图.
- **网格（Mesh）/Primitive**：一个 Mesh 可包含多个 Primitive（不同材质/拓扑）.
- **材质（Material）**：PBR 金属度/粗糙度工作流（baseColor、metallicRoughness、normal、occlusion…）.
- **纹理（Texture/Image/Sampler）**：图像数据与采样方式.
- **相机与灯光**：本项目从 glTF 中读取，并用于初始化相机与阴影系统.

### Model 的设计：把 glTF 解析成“渲染可用”的资源池

`Model`（`code/Inc/model.h`、`code/Src/model.cpp`）是一个资源加载与缓存容器：
- `meshes[]`：每个 Mesh（准确说是 glTF primitive）会被解析成一个 `Mesh` 对象（VAO/VBO/EBO 由 `Mesh::setupMesh()` 创建）.
- `materials[]`：解析 glTF material，映射到 `PBRMaterial`（`code/Inc/pbr_material.h`）.
- `textures[]`：纹理池（`Texture2D`）供材质索引引用.
- `modelNodes[]`：将 glTF scene graph 的节点转换成 `ModelNode`，保存 `localTransform/globalTransform` 与 `meshIndex`.
- `cameras[]` / `lights_[]`：从 glTF 提取的相机与灯光数据.

一个值得注意的工程取舍：
- 解析阶段把场景边界（AABB）累积到世界空间（`sceneMin_`/`sceneMax_`），用于阴影系统自适应正交体范围（见“阴影系统”）.

### Node 与 Mesh 的映射方式

`Model::ProcessNode()` 会递归遍历 glTF 节点，并把 TRS 组合成矩阵：
- `localTransform = ComposeTRS(node)`
- `globalTransform = parentTransform * localTransform`

当遇到 `node.mesh` 时，会遍历其中的 primitives，并为每个 primitive 生成一个 `Mesh`：
- `ProcessMesh(..., worldTransform)` 读取 POSITION/NORMAL/TEXCOORD_0/1/TANGENT 等 attribute
- 计算 tangent space 相关数据（bitangent 由 handedness 与 cross 得到）
- 更新世界空间 AABB

这使得后续渲染侧可以：
- 通过 `ModelNode::meshIndex` 快速定位 `Model::meshes[]`
- 通过 `Mesh::GetMaterialIndex()` 快速定位 `Model::materials[]`

### 材质/纹理：PBRMaterial 与 KHR_texture_transform

`PBRMaterial` 在工程中承担“glTF material 的可渲染形态”，并额外支持：
- **多套 UV**：`texCoord` 可选择 TEXCOORD_0 或 TEXCOORD_1
- **KHR_texture_transform**：把 offset/scale/rotation（以及可能的 texCoord 覆盖）解析到 `PBRTextureInfo`

在渲染阶段，`Shader::SetMaterial()`（`code/Src/shader.cpp`）会把材质参数与纹理绑定到固定纹理单元，并设置若干 uniform：
- `hasBaseColorTex/hasMetalRoughTex/hasNormalTex/hasOcclusionTex`
- `*_texCoordSet` + `*_hasTransform` + `*_offset/scale/rotation`

对应的片段着色器 `code/Shaders/mesh_pbr.frag` 中包含了两段关键逻辑：
- 根据 `*_texCoordSet` 选择 `vUV` 或 `vUV2`
- 通过 `ApplyTexTransform()` 将 KHR_texture_transform 应用到 uv

---

## 相机：FPS 控制与 glTF 初始化

### FPS 相机的输入映射

`Camera`（`code/Inc/camera.h`、`code/Src/camera.cpp`）遵循常见 FPS 约定：
- 键盘移动：沿 `front/right/world_up` 方向做位移（速度随 `deltaTime` 缩放）
- 鼠标控制：xoffset/yoffset 更新 yaw/pitch，并限制 pitch 防止万向节锁
- 滚轮：调整 FOV，并在 `MIN_FOV/MAX_FOV` 之间夹紧

`App` 中开启 `GLFW_CURSOR_DISABLED`，因此鼠标将被锁定在窗口内（更贴近 FPS 交互）.

### 相机初始状态来自 glTF

`Renderer` 构造时：
- 创建 `Model("../resources/blender/pools.glb")`
- 通过 `Model::GetModelCamInfos()` 取 **第一台** glTF 相机作为默认相机参数
- `camera = std::make_unique<Camera>(model->GetModelCamInfos())`

当前实现固定选择 glTF 中的第一台相机作为初始视角.若 glTF 文件中包含多相机且需要在运行时切换，可在 `Renderer` 中暴露相机索引或名称选择机制.

### 水面反射相机

水面反射 pass 需要一个“关于水面平面镜像”的相机：
- `Camera::MakeReflectedCamera(waterHeight)` 通过将相机位置与视线方向关于水面平面做镜像来构造反射相机（实现上可等效为镜像位置并调整俯仰角/朝向）

这一设计将“反射相机逻辑”封装在相机内部，使 `Renderer::Render()` 能直接以相同接口渲染反射 pass.

---

## 可渲染对象体系：IRenderable、SceneObject、WaterSurface、Skybox

### 统一接口：IRenderable

`IRenderable`（`code/Inc/irenderable.h`）提供了最小生命周期：
- `Init()`：创建 GPU 资源、编译 shader、加载纹理等
- `Update(dt)`：推进动画/状态
- `Render(camera, aspect)`：提交绘制

这种接口有利于 `Renderer` 按模块组织更新与渲染，而不需要关心具体类型.

### SceneObject：静态物体的默认实现

`SceneObject`（`code/Inc/scene_object.h`、`code/Src/scene_object.cpp`）是“非水面”的默认渲染单元：
- 每个实例绑定一个 glTF node（`rootNodeIndex`），并渲染该节点对应的 mesh
- 使用 PBR shader：`code/Shaders/mesh_pbr.vert` + `code/Shaders/mesh_pbr.frag`
- 同时持有阴影深度 shader：`code/Shaders/shadow_depth.vert` + `code/Shaders/shadow_depth.frag`

它还提供了 **实例级** TRS（position/rotation/scale），与 glTF node 的 `localTransform` 组合形成最终模型矩阵：

```cpp
glm::mat4 instanceM = ComputeModelMatrix();
glm::mat4 globalXform = instanceM * node.localTransform;
```

因此：glTF 的变换负责“资产自带的摆放”，而 `SceneObject` 的 TRS 负责“运行时实例调整”.

### WaterSurface：水面作为特殊的 SceneObject

`WaterSurface` 继承自 `SceneObject`（`code/Inc/water_surface.h`、`code/Src/water_surface.cpp`），但改用独立 shader 与渲染输入：
- 需要 `Renderer` 注入的反射/折射颜色纹理与折射深度纹理
- 需要水面高度与裁剪平面
- 有自己的 DUDV 纹理与扰动参数

### Skybox：背景渲染模块

`Skybox` 直接实现 `IRenderable`（`code/Inc/skybox.h`、`code/Src/skybox.cpp`），支持两种 cubemap 来源：
- 优先使用 `LightingSystem` 生成的 HDR 环境 cubemap
- 若不可用，回退加载 `resources/skybox/*.jpg`

---

## 天空盒：无穷远的“视差消除”与深度技巧

天空盒的视觉目标是“无穷远背景”，其核心技巧是：
1) **从 view 矩阵移除平移分量**：只保留旋转，让天空盒不随相机平移产生视差
2) **深度测试策略**：渲染天空盒时设置 `glDepthFunc(GL_LEQUAL)`，并在 shader 中使用 `pos.xyww`，使天空盒深度恒为 1（在最远处）

在 `code/Shaders/skybox.vert` 中可以看到典型写法：

```glsl
mat4 rotView = mat4(mat3(view));
vec4 pos = projection * rotView * vec4(aPos, 1.0);
gl_Position = pos.xyww;
```

这会让天空盒“看起来”固定在相机周围的无限远处，同时不会遮挡真实几何体.

---

## 光照系统：从直接光到 IBL（环境光照）

`LightingSystem`（`code/Inc/lighting_system.h`、`code/Src/lighting_system.cpp`）同时承担两类职责：
- 管理实时光源数组（平行光/点光/聚光）并绑定到 PBR shader
- 管理 HDR 环境贴图与 IBL 预计算（cubemap、irradiance、prefilter、BRDF LUT）

### glTF 灯光的引入

`Renderer::Init()` 调用：
- `lightingSystem->LoadLightsFromModel(model.get())`

为了避免 glTF 灯光强度与引擎期望不一致，工程中对强度做了缩放：
- `gltf_intensityScale`（`code/Src/lighting_system.cpp`）

### HDR → Cubemap：GPU 转换

当加载 `resources/hdr/*.hdr` 时，系统执行：
1) `LoadHDRTexture()`：读入 equirectangular HDR（2D）到 `GL_RGB16F`
2) `ConvertHDRToCubemap()`：用 `cubemap.vert` + `equirectangular_to_cubemap.frag` 渲染到 cubemap 六个面

`equirectangular_to_cubemap.frag` 使用经纬度采样（spherical mapping）将方向向量映射到 2D HDR 纹理坐标.

### IBL 预计算：irradiance / prefilter / BRDF LUT

当环境 cubemap 准备好后，会继续生成三类贴图：
- **Diffuse irradiance**：`irradiance_convolution.frag` 对半球方向积分，生成低频漫反射环境
- **Specular prefilter**：`prefilter.frag` 使用 GGX 重要性采样，并为不同 roughness 写入不同 mip level
- **BRDF LUT**：`brdf.frag` 预积分得到二维 LUT（NdotV × roughness）

在 PBR shader 中，这三者分别承担：
- diffuse：`irradianceMap(N)`
- specular：`prefilterMap(R, mip=roughness)`
- energy compensation：`brdfLUT(NdotV, roughness)`

工程中的纹理单元绑定约定（在 `LightingSystem::BindLightingToShader()`）：
- `irradianceMap` → unit 10
- `prefilterMap` → unit 11
- `brdfLUT` → unit 12

这些单元刻意与材质贴图（baseColor/normal/metallicRoughness 等）的常用绑定区间分离，以避免纹理单元冲突并简化调试.

---

## PBR 材质：Cook-Torrance（GGX）与法线贴图

静态物体使用 `mesh_pbr.vert/.frag`.

### 顶点阶段：世界空间与切线空间

`mesh_pbr.vert` 输出：
- `vWorldPos`、`vNormal`
- `vTangent`、`vBitangent`（用于构建 TBN）
- `vLightSpacePos`（用于阴影）

法线/切线/副切线都通过 normal matrix（`transpose(inverse(model))`）变换到世界空间，保证非一致缩放下仍正确.

### 片段阶段：TBN + normal map

在 `mesh_pbr.frag` 中，若 `hasNormalTex == 1`，会读取法线贴图并从切线空间变换到世界空间：
- 读取法线贴图 `rgb` 并映射到 [-1, 1]
- `worldNormal = normalize(TBN * normalFromMap)`

这使得微表面方向随法线贴图变化，进一步影响 N·L、N·V、反射向量等，体现细节起伏.

### 金属度/粗糙度工作流

本项目沿用 glTF 的 metallic-roughness 纹理通道约定：
- `mr.g` → roughness
- `mr.b` → metallic

并与材质因子相乘：
- `metallic *= mr.b`
- `roughness *= mr.g`

### 多光源 + IBL 的组合

`mesh_pbr.frag` 先计算直接光（多光源循环，含点光/聚光衰减），再在 `hasIBL==1` 时叠加 IBL 的 diffuse/specular.

这种组合的关键点在于：
- 直接光提供“局部可控”的高频照明与阴影
- IBL 提供来自环境的整体能量与方向性反射（尤其对金属材质影响显著）

---

## 帧缓冲与水面：反射/折射 Pass、Fresnel、DUDV 扰动

### Framebuffer 封装

`Framebuffer`（`code/Inc/framebuffer.h`、`code/Src/framebuffer.cpp`）是一个轻量 RAII 封装：
- color：`GL_RGBA` 颜色纹理
- depth：`GL_DEPTH_COMPONENT24` 深度纹理
- 支持 `Resize()`（窗口大小变化时重建 attachment）

### 多 pass 渲染顺序

在 `Renderer::Render()`（`code/Src/renderer.cpp`）中，典型顺序为：
1) Shadow pass（若存在方向光）
2) Reflection pass（裁剪水面下方）
3) Refraction pass（裁剪水面上方）
4) Final scene pass（主画面）
5) Water pass（最后绘制以利用透明混合）

裁剪由 OpenGL 的 clip distance 实现：
- `glEnable(GL_CLIP_DISTANCE0)`
- 顶点 shader 写 `gl_ClipDistance[0] = dot(worldPos, clipPlane)`

### 水面识别与裁剪平面

`Renderer` 在遍历 `ModelNode` 时按名字前缀识别水面：
- `matchPrefixIgnoreCase(node.name, "water")` → 创建 `WaterSurface`

`WaterSurface::ComputeClipPlanes()` 通过统计水面网格顶点的世界空间 y 均值，得到 `waterHeight_`，进而构建：
- 反射裁剪平面：保留水面上方 `y > waterHeight`
- 折射裁剪平面：保留水面下方 `y < waterHeight`

### Water shader：折射方向、Fresnel 与深度厚度

`code/Shaders/water.frag` 的关键点包括：
- 由 DUDV + 简化波浪生成扰动法线（不使用传统法线贴图）
- 用 `refract(I, N, 1/eta)` 的折射方向调节折射采样扰动
- Fresnel：以 Schlick 形式从 $F_0$ 推导视角相关混合系数
- 深度：从折射 pass 的深度纹理取样并线性化，得到“水体厚度”，再用 Beer–Lambert 指数衰减模拟吸收

水面最终颜色是：
- 折射颜色（经过吸收/散射修正）与反射颜色按 Fresnel 混合

---

## 阴影系统：Shadow Pass、Shadow Map 与 PCF

`ShadowSystem`（`code/Inc/shadow_system.h`、`code/Src/shadow_system.cpp`）以“方向光阴影贴图”为主：
- 仅当 `LightingSystem` 中存在方向光时启用阴影
- 当前实现仅支持方向光的阴影贴图（正交投影），点光与聚光阴影未实现

### 光空间矩阵：按场景边界自适应

阴影覆盖范围由 `Model` 的世界空间 AABB 推导：
- `sceneCenter` / `sceneRadius` 来自 `Model::GetSceneCenter/Radius()`

`ShadowSystem::UpdateLightSpaceMatrix()` 将光源“相机”放在场景中心沿光线反方向若干个半径处，并构建：
- `lightView = lookAt(lightPos, sceneCenter, up)`
- `lightProjection = ortho(-orthoSize, orthoSize, ..., near, far)`

这种做法的优势是：场景尺度变化时无需手调正交体范围.

### Shadow pass：只写深度

Shadow pass 绑定 `shadowFBO_`，并禁用颜色写入：
- `glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE)`
- 使用 polygon offset 缓解 self-shadowing（shadow acne）

实际几何由 `SceneObject::RenderShadowDepth()` 使用 `shadow_depth.*` shader 绘制.

### 主 pass 采样：PCF 软阴影

`mesh_pbr.frag` 中实现了 PCF：对 shadow map 进行 3×3 采样并平均，得到软化边缘的阴影.

（代码中还有）自适应 bias：根据表面法线与光线夹角调整深度偏移，减少 acne 与 peter-panning.

---

## 常见扩展点与工程习惯

### 添加新的可渲染类型

工程倾向于：
- 对“普通静态物体”直接使用/派生 `SceneObject`
- 对“特殊材质/特殊 pass”的物体（如水面）创建单独类与 shader

将新模块接入渲染通常需要：
1) 在 `Renderer` 构造或 `Init()` 中创建实例并加入渲染列表
2) 在 `Init()` 中为其注入 `LightingSystem`、`ShadowSystem`（若需要）
3) 在 `Render()` 中选择合适的 pass 或渲染顺序

### 资源与 shader 变更的落点

- 替换模型：主要改动在 `Renderer` 中的 glTF 路径，以及可能的 node 命名约定（例如水面前缀）.
- 增加/修改材质语义：优先从 `PBRMaterial`/`Shader::SetMaterial()` 与 `mesh_pbr.frag` 的 uniform 设计入手.
- 调整 IBL：落点集中在 `LightingSystem`（贴图分辨率、采样数、绑定单元）与相关 shader（prefilter/brdf/irradiance）.

---

## 读代码的推荐入口（按“理解成本”排序）

1) 运行循环与输入：`code/Src/app.cpp`
2) 渲染管线组织：`code/Src/renderer.cpp`
3) glTF 解析与资源池：`code/Src/model.cpp`
4) 直接光 + IBL：`code/Src/lighting_system.cpp` + `code/Shaders/*`
5) 阴影：`code/Src/shadow_system.cpp` + `code/Shaders/shadow_depth.*` + `code/Shaders/mesh_pbr.frag`
6) 水面：`code/Src/water_surface.cpp` + `code/Shaders/water.*`
7) 天空盒：`code/Src/skybox.cpp` + `code/Shaders/skybox.*`

---

## 附：关键 shader 文件索引

- PBR：`code/Shaders/mesh_pbr.vert`、`code/Shaders/mesh_pbr.frag`
- 水面：`code/Shaders/water.vert`、`code/Shaders/water.frag`
- 天空盒：`code/Shaders/skybox.vert`、`code/Shaders/skybox.frag`
- 阴影深度：`code/Shaders/shadow_depth.vert`、`code/Shaders/shadow_depth.frag`
- IBL 预计算：
  - `code/Shaders/equirectangular_to_cubemap.frag`
  - `code/Shaders/irradiance_convolution.frag`
  - `code/Shaders/prefilter.frag`
  - `code/Shaders/brdf.vert`、`code/Shaders/brdf.frag`
  - `code/Shaders/cubemap.vert`





> 🤖 **本文部分技术说明与措辞由人工智能工具在作者提供的代码与要点基础上辅助生成，作者已对内容进行校对与修改，并对最终文本负责**
