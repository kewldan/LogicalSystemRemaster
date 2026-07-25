#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aInfo;
layout (location = 3) in ivec2 aGrid;

out vec3 v_texCoord;
out float v_state;
out float v_selection;

uniform mat4 proj, view;

const mat2 rotations[4] = mat2[4](
    mat2(1.0, 0.0, 0.0, 1.0),
    mat2(0.0, -1.0, 1.0, 0.0),
    mat2(-1.0, 0.0, 0.0, -1.0),
    mat2(0.0, 1.0, -1.0, 0.0)
);

void main()
{
    vec2 local = rotations[(aInfo >> 6) & 3] * (aPos.xy - 0.5);
    vec2 world = (vec2(aGrid) + local) * 32.0;
    gl_Position = proj * view * vec4(world, -0.2, 1.0);
    v_texCoord = vec3(aTexCoord, float((aInfo >> 2) & 15));
    v_texCoord.y = -v_texCoord.y;
    v_state = ((aInfo & 2) != 0) ? 1.0 : 0.0;
    v_selection = ((aInfo & 1) != 0) ? 1.0 : 0.0;
}
