#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;

void main()
{
    gAlbedo = vec4(0.01, 0.6, 1, 0.1);
    gAlbedoHDR = vec4(0, 0, 0, 1);
}
