#pragma once
#include <string>
#include <glad/glad.h>

class Texture2D {
public:
    Texture2D();
    ~Texture2D();

    // 禁止拷贝，允许移动
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    bool LoadFromMemory(
        const unsigned char* data,
        int width,
        int height,
        int components,
        bool srgb);

    bool LoadFromFile(const std::string& filepath, bool srgb);

    void Bind(int unit) const;
    GLuint GetID() const { return texID; }
    bool IsValid() const { return texID != 0; }

private:
    GLuint texID = 0;
};