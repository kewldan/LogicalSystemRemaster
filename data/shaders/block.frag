#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;
  
in Vertex {
    vec2 texCoord;
    float id;
    float state;
} vertex;

uniform sampler2DArray tex;

void main()
{
    gAlbedo = texture(tex, vec3(vertex.texCoord, vertex.id));
    if(gAlbedo.a == 0){
        if(vertex.state == 1){
            gAlbedo.rgb = vec3(0.33, 0.9, 0.27);
        }else{
            gAlbedo.rgb = vec3(0.92, 0.31, 0.2);
        }
    }
    gAlbedoHDR = vec4(2 * gAlbedo.rgb * vertex.state * (1 - gAlbedo.a), 1.0);
}
