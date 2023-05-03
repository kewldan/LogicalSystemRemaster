#version 330

out vec4 FragColor;

in vec2 TexCoords;

uniform vec4 color;
uniform vec2 uv;
uniform sampler2D tex;
uniform vec2 size;

const float width = 0.5;
const float edge = 1.0/8.0;

void main() {
    vec2 t = vec2(uv.x, uv.y) + vec2(TexCoords.x, 1.0 - TexCoords.y) * vec2(size.x, size.y);
    float distance = 1.0 - texture(tex, t).a;
    float alpha = 1.0 - smoothstep(width - edge, width + edge, distance);
    FragColor = vec4(color.rgb, color.a * alpha);
}
