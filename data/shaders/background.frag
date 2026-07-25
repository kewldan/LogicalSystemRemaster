#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;

in vec2 pixel;

uniform vec2 offset;

void main()
{
    if ((mod(pixel.x + offset.x, 64.0) > 32.0) != (mod(pixel.y + offset.y, 64.0) > 32.0)){
        gAlbedo = vec4(0.2, 0.202, 0.23, 1.0);
    } else {
        gAlbedo = vec4(0.17, 0.18, 0.2, 1.0);
    }
    gAlbedoHDR = vec4(0.0, 0.0, 0.0, 1.0);
}
