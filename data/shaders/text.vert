#version 330

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec2 tex_coord;
layout (location = 2) in vec4 color;

uniform mat4 mvp, view, proj;

out vec2 tex;
out vec4 col;

void main()
{
    tex = tex_coord;
    col = color;
    gl_Position       = proj*(view*(mvp*vec4(vertex,1.0)));
}