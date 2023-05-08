#version 330
out vec4 gAlbedo;
out vec4 gAlbedoHDR;

in Vertex {
    vec3 texCoord;
    float state;
    float selection;
} vertex;

uniform sampler2DArray tex;
uniform vec3 selectionColor, ON, OFF;

#define BLOOM_STRENGTH 2

void main()
{
    gAlbedo = texture(tex, vertex.texCoord);
    gAlbedo.rgb = mix(mix(OFF, ON, vertex.state), gAlbedo.rgb, gAlbedo.a);
    gAlbedo.a = 1;
    gAlbedo *= mix(vec4(1), vec4(selectionColor, 1), vertex.selection);
    gAlbedoHDR = vec4(0, 0, 0, 1);
}
