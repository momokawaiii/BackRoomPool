#include "shader.h"

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // 1. retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        // open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();
    // 2. compile shaders
    unsigned int vertex, fragment;
    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");
    // fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");
    // shader Program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");
    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use()
{
    glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& vec) const
{
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec));
}

void Shader::setVec3(const std::string& name, const glm::vec3& vec) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec));
}

void Shader::setVec4(const std::string& name, const glm::vec4& vec) const
{
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec));
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::SetMaterial(const PBRMaterial& mat, const Model& model)
{
    use(); // 确保当前 shader 激活

    //
    // ======== 基础参数 ========
    //
    setVec4("baseColorFactor", mat.baseColorFactor);
    setFloat("metallicFactor", mat.metallicFactor);
    setFloat("roughnessFactor", mat.roughnessFactor);
    setFloat("occlusionStrength", mat.occlusionStrength);

    //
    // ======== baseColorTexture （含 transform）========
    //
    if (mat.baseColorTexture.hasTexture()) {
        int tid = mat.baseColorTexture.textureIndex;
        model.GetTextures()[tid].Bind(0);

        setInt("baseColorTex", 0);
        setInt("hasBaseColorTex", 1);
        setInt("baseColor_texCoordSet", mat.baseColorTexture.texCoord); // 设置纹理坐标集索引

        // transform uniforms
        setInt("baseColor_hasTransform", mat.baseColorTexture.hasTransform ? 1 : 0);
        setVec2("baseColor_offset", mat.baseColorTexture.offset);
        setVec2("baseColor_scale", mat.baseColorTexture.scale);
        setFloat("baseColor_rotation", mat.baseColorTexture.rotation);
    }
    else {
        setInt("hasBaseColorTex", 0);
        setInt("baseColor_hasTransform", 0);
        setInt("baseColor_texCoordSet", 0);
    }

    //
    // ======== metallic-roughness texture（含 transform）========
    //
    if (mat.metallicRoughnessTexture.hasTexture()) {
        int tid = mat.metallicRoughnessTexture.textureIndex;
        model.GetTextures()[tid].Bind(1);

        setInt("metalRoughTex", 1);
        setInt("hasMetalRoughTex", 1);
        setInt("metalRough_texCoordSet", mat.metallicRoughnessTexture.texCoord); // 设置纹理坐标集索引

        setInt("metalRough_hasTransform", mat.metallicRoughnessTexture.hasTransform ? 1 : 0);
        setVec2("metalRough_offset", mat.metallicRoughnessTexture.offset);
        setVec2("metalRough_scale", mat.metallicRoughnessTexture.scale);
        setFloat("metalRough_rotation", mat.metallicRoughnessTexture.rotation);
    }
    else {
        setInt("hasMetalRoughTex", 0);
        setInt("metalRough_hasTransform", 0);
        setInt("metalRough_texCoordSet", 0);
    }

    //
    // ======== normal map =========
    //
    if (mat.normalTexture.hasTexture()) {
        int tid = mat.normalTexture.textureIndex;
        model.GetTextures()[tid].Bind(2);

        setInt("normalTex", 2);
        setInt("hasNormalTex", 1);
        setInt("normal_texCoordSet", mat.normalTexture.texCoord); // 设置纹理坐标集索引

        // normal map 也可以支持 transform（如你的 glTF 里启用）
        setInt("normal_hasTransform", mat.normalTexture.hasTransform ? 1 : 0);
        setVec2("normal_offset", mat.normalTexture.offset);
        setVec2("normal_scale", mat.normalTexture.scale);
        setFloat("normal_rotation", mat.normalTexture.rotation);
    }
    else {
        setInt("hasNormalTex", 0);
        setInt("normal_hasTransform", 0);
        setInt("normal_texCoordSet", 0);
    }

    //
    // ======== occlusion map =========
    //
    if (mat.occlusionTexture.hasTexture()) {
        int tid = mat.occlusionTexture.textureIndex;
        model.GetTextures()[tid].Bind(3);

        setInt("occlusionTex", 3);
        setInt("hasOcclusionTex", 1);
        setInt("occlusion_texCoordSet", mat.occlusionTexture.texCoord); // 设置纹理坐标集索引

        // occlusion map 支持 texture transform
        setInt("occlusion_hasTransform", mat.occlusionTexture.hasTransform ? 1 : 0);
        setVec2("occlusion_offset", mat.occlusionTexture.offset);
        setVec2("occlusion_scale", mat.occlusionTexture.scale);
        setFloat("occlusion_rotation", mat.occlusionTexture.rotation);
    }
    else {
        setInt("hasOcclusionTex", 0);
        setInt("occlusion_hasTransform", 0);
        setInt("occlusion_texCoordSet", 0);
    }
}

void Shader::checkCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}