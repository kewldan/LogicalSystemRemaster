#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;

uniform vec2 offset;

void main()
{
    if(mod(gl_FragCoord.x + offset.x + 16, 64) > 32 ^^ mod(gl_FragCoord.y + offset.y + 16, 64) > 32){
        gAlbedo = vec4(0.2, 0.202, 0.23, 1);
    }else{
        gAlbedo = vec4(0.17, 0.18, 0.2, 1);
    }
    gAlbedoHDR = vec4(0, 0, 0, 1);
}
