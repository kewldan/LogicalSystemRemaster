#version 330
out vec4 gAlbedoHDR;

in Vertex {
    vec3 texCoord;
    float state;
    float selection;
} vertex;

uniform sampler2DArray tex;
uniform vec3 ON, OFF;
#define BLOOM_STRENGTH 2

void main()
{
    vec4 gAlbedo = texture(tex, vertex.texCoord);
    if(gAlbedo.a == 0){
        gAlbedo.rgb = mix(mix(OFF, ON, vertex.state), gAlbedo.rgb, gAlbedo.a);
        gAlbedoHDR = vec4(BLOOM_STRENGTH * gAlbedo.rgb * vertex.state * (1 - gAlbedo.a), 1.0);
    }
}
