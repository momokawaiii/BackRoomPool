#include "texture2d.h"
#include <iostream>
#include <stb_image.h>

Texture2D::Texture2D() {}

Texture2D::~Texture2D() {
    if (texID != 0) {
        glDeleteTextures(1, &texID);
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept {
    texID = other.texID;
    other.texID = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        if (texID != 0) glDeleteTextures(1, &texID);
        texID = other.texID;
        other.texID = 0;
    }
    return *this;
}

bool Texture2D::LoadFromMemory(
    const unsigned char* data,
    int width,
    int height,
    int components,
    bool srgb)
{
    if (!data || width <= 0 || height <= 0) {
        std::cerr << "[Texture2D] Invalid image data.\n";
        return false;
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB8;   // default linear RGB

    if (components == 1) {
        format = GL_RED;
        internalFormat = GL_R8;        // single channel linear
    }
    else if (components == 2) {
        format = GL_RG;
        internalFormat = GL_RG8;       // two channel linear
    }
    else if (components == 3) {
        format = GL_RGB;
        internalFormat = srgb ? GL_SRGB8 : GL_RGB8;   // 3-channel sRGB or linear
    }
    else if (components == 4) {
        format = GL_RGBA;
        internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8; // 4-channel sRGB or linear
    }

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0,
        internalFormat,
        width, height, 0,
        format, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    // default glTF wrapping & filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return true;
}

bool Texture2D::LoadFromFile(const std::string& path, bool srgb) {
    int width = 0;
    int height = 0;
    int components = 0;

    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &components, 0);
    if (!data) {
        std::cerr << "[Texture2D] Failed to load image from file: " << path << std::endl;
        return false;
    }

    bool ok = LoadFromMemory(data, width, height, components, srgb);
    stbi_image_free(data);
    return ok;
}

void Texture2D::Bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texID);
}