#version 300 es
precision mediump float;

in vec2 texCoords;
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
out vec4 FragColor;

void main()
{    
    FragColor = vec4(vec3(1, 0, 0), 1.);
}