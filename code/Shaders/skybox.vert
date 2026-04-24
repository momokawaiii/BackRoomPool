#version 410 core
layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 TexCoords;

void main()
{
    TexCoords = aPos;

    // 移除view矩阵的平移部分，只保留旋转
    mat4 rotView = mat4(mat3(view));
    vec4 pos = projection * rotView * vec4(aPos, 1.0);

    // 裁剪处理：将天空盒方向转换到世界空间进行裁剪
    vec3 worldDirection = inverse(mat3(view)) * aPos;
    vec3 worldPos = worldDirection * 1000.0;  // 模拟无穷远

    gl_Position = pos.xyww;
}
