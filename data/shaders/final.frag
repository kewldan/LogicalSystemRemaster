#version 330
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float bloom;

void main()
{
    vec4 hdrColor = texture(scene, TexCoords);
    vec4 bloomColor = texture(bloomBlur, TexCoords);
    FragColor = hdrColor + bloomColor * bloom;
}
