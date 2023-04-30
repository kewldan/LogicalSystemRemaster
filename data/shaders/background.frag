#version 330
out vec4 gAlbedo;

uniform vec2 offset, horizontal, vertical, screen;

float map(float value, float min1, float max1, float min2, float max2) {
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

void main()
{
    if (mod(map(gl_FragCoord.x, 0, screen.x, horizontal.x, horizontal.y) + offset.x + 16, 64) > 32 ^^ mod(map(gl_FragCoord.y, 0, screen.y, vertical.x, vertical.y) + offset.y + 16, 64) > 32){
        gAlbedo = vec4(0.2, 0.202, 0.23, 1);
    } else {
        gAlbedo = vec4(0.17, 0.18, 0.2, 1);
    }
}
