#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iChannel0;
out vec4 FragColor;

void main()
{    
    FragColor = texture(iChannel0, texCoords);
}