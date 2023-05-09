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
    gAlbedoHDR = vec4(mix(vec3(0), ON * 2, (1 - gAlbedo.a) * vertex.state), 1.0);
    gAlbedo.a = 1.0;
    gAlbedo.rgb *= mix(vec3(1.0), selectionColor, vertex.selection);
}
