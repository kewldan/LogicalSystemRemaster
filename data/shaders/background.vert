#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 pixel;

uniform vec2 horizontal, vertical;

float map(float value, float min, float max) {
    return min + value * (max - min);
}

void main()
{
    pixel = vec2(map(aTexCoord.x, horizontal.x, horizontal.y), map(aTexCoord.y, vertical.x, vertical.y));
    gl_Position = vec4(aPos, 1.0);
}
