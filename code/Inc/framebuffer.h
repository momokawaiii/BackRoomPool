#pragma once

#include <glad/glad.h>

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    // 禁止拷贝，允许移动（避免重复释放）
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    // 初始化 / 重新创建
    void Init(int width, int height);
    void Resize(int width, int height);

    // 绑定到当前渲染目标
    void Bind();
    static void Unbind(); // 绑定回默认Framebuffer(0)

    // 获取纹理句柄
    GLuint GetColorTexture() const { return colorTex_; }
    GLuint GetDepthTexture() const { return depthTex_; }

    int GetWidth()  const { return width_; }
    int GetHeight() const { return height_; }

protected:
    // 允许子类扩展 attachment 逻辑
    virtual void CreateAttachments();
    virtual void Destroy();

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint depthTex_ = 0;
    int width_ = 0;
    int height_ = 0;
};
