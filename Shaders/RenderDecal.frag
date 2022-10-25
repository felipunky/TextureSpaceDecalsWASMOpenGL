#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
out vec4 FragColor;

void main()
{    
    vec4 decals = texture(iChannel1, texCoords);
    vec4 render = texture(iChannel2, texCoords);
    FragColor = vec4(decals.rgb + render.rgb * decals.a, 1.);
}