#version 330

out vec4 gAlbedo;

uniform sampler2D texture;

in vec2 tex;
in vec4 col;

void main()
{
    float a = texture2D(texture, tex).r;
    gAlbedo = vec4(col.rgb, col.a*a);
}