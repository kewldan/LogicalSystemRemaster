#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aInfo;
layout (location = 3) in mat4 aMvp;

out Vertex {
    vec3 texCoord;
    float state;
    float selection;
} vertex;

uniform mat4 proj, view;

void main()
{
    gl_Position = proj * view * aMvp * vec4(aPos, 1.0);
    vertex.texCoord = vec3(aTexCoord, aInfo >> 2);
    vertex.texCoord.y = -vertex.texCoord.y;
    vertex.state = ((aInfo & 2) != 0) ? 1.0 : 0.0;
    vertex.selection = ((aInfo & 1) != 0) ? 1.0 : 0.0;
}
