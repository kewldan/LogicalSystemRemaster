#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;
  
in Vertex {
    vec3 texCoord;
    float state;
    float selection;
} vertex;

uniform sampler2DArray tex;
uniform vec3 selectionColor;

#define OFF vec3(0.92, 0.31, 0.2)
#define ON vec3(0.33, 0.9, 0.27)
#define BLOOM_STRENGTH 2

void main()
{
    gAlbedo = texture(tex, vertex.texCoord);
    gAlbedo.rgb = mix(mix(OFF, ON, vertex.state), gAlbedo.rgb, gAlbedo.a);
    gAlbedoHDR = vec4(BLOOM_STRENGTH * gAlbedo.rgb * vertex.state * (1 - gAlbedo.a), 1.0);
    gAlbedo *= mix(vec4(1), vec4(selectionColor, 1), vertex.selection);
}
