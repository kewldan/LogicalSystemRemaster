#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec2 aId;
layout (location = 3) in mat4 aMvp;

out Vertex {
    vec2 texCoord;
    float id;
    float state;
} vertex;

uniform mat4 proj, view;

void main()
{
    gl_Position = proj * view * aMvp * vec4(aPos, 1.0);
    vertex.texCoord = aTexCoord;
    vertex.texCoord.y = -vertex.texCoord.y;
    vertex.id = aId.x;
    vertex.state = aId.y;
}
