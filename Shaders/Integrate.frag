#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iDepth;
out vec4 FragColor;

void main()
{    
    vec4 tex = texture(iDepth, texCoords);
    FragColor = tex;
}