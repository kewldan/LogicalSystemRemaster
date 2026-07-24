#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aInfo;
layout (location = 3) in ivec2 aGrid;

out Vertex {
    vec3 texCoord;
    float state;
    float selection;
} vertex;

uniform mat4 proj, view;

// info bits: 0 - selected, 1 - active, 2..5 - type id, 6..7 - rotation
const mat2 rotations[4] = mat2[4](
    mat2(1.0, 0.0, 0.0, 1.0),   // up
    mat2(0.0, -1.0, 1.0, 0.0),  // right (-90 deg)
    mat2(-1.0, 0.0, 0.0, -1.0), // down (180 deg)
    mat2(0.0, 1.0, -1.0, 0.0)   // left (90 deg)
);

void main()
{
    vec2 local = rotations[(aInfo >> 6) & 3] * (aPos.xy - 0.5);
    vec2 world = (vec2(aGrid) + local) * 32.0;
    gl_Position = proj * view * vec4(world, -0.2, 1.0);
    vertex.texCoord = vec3(aTexCoord, (aInfo >> 2) & 15);
    vertex.texCoord.y = -vertex.texCoord.y;
    vertex.state = ((aInfo & 2) != 0) ? 1.0 : 0.0;
    vertex.selection = ((aInfo & 1) != 0) ? 1.0 : 0.0;
}
