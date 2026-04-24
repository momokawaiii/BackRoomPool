#version 410 core

// 深度渲染片段着色器
// 不需要输出颜色，只写入深度信息

void main()
{
    // 空的片段着色器
    // OpenGL 会自动处理深度写入
    // 可以在这里添加 alpha test 或其他深度相关的处理

    // 对于不透明物体，直接让OpenGL写入深度
    // 如果需要处理透明物体，可以在这里添加 alpha test:
    // if (alpha < 0.5) discard;
}