#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;

in vec2 texCoord;

uniform vec2 size;
uniform float width;
uniform vec4 color;
uniform vec4 borderColor;

void main()
{
    vec2 borderWidth = width / size;
    float maxX = 1.0 - borderWidth.x;
    float minX = borderWidth.x;
    float maxY = (1.0 - borderWidth.y);
    float minY = borderWidth.y;

    if (texCoord.x < maxX && texCoord.x > minX &&
    texCoord.y < maxY && texCoord.y > minY) {
        gAlbedo = color;
    } else {
        gAlbedo = borderColor;
    }
    gAlbedoHDR = vec4(0, 0, 0, 1);
}
