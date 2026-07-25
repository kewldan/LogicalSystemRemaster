#version 330
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gAlbedoHDR;

in vec3 v_texCoord;
in float v_state;
in float v_selection;

uniform highp sampler2DArray tex;
uniform vec3 selectionColor, ON, OFF;
uniform float alpha;

void main()
{
    gAlbedo = texture(tex, v_texCoord);
    gAlbedo.rgb = mix(mix(OFF, ON, v_state), gAlbedo.rgb, gAlbedo.a);
    gAlbedoHDR = vec4(mix(vec3(0.0), ON * 2.0, (1.0 - gAlbedo.a) * v_state), 1.0);
    gAlbedo.a = alpha;
    gAlbedo.rgb *= mix(vec3(1.0), selectionColor, v_selection);
}
