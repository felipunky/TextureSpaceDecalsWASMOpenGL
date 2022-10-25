#version 300 es
precision mediump float;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoords;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 

void main()
{
    vec4 worldPos = vec4(Position, 1.0);
    worldPos = view * worldPos;
    worldPos = projection * worldPos;  
    gl_Position = worldPos;  
}