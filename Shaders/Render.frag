#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iChannel0;
out vec4 FragColor;

void main()
{    
    vec4 tex = texture(iChannel0, texCoords);
    FragColor = vec4(pow(tex.rgb, vec3(2.2)), tex.a);
}