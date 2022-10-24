#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iChannel1;
out vec4 FragColor;

void main()
{    
    FragColor = texture(iChannel1, texCoords);
}