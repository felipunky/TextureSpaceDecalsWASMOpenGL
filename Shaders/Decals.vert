#version 300 es
precision mediump float;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoords;

out vec2 texCoords;
out vec3 iPosition;
out vec4 positionCS;
out vec4 positionVS;
out vec3 rayPos;
out vec4 FS_IN_ClipPos;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 

uniform vec3 camPos;

void main()
{
    texCoords = TexCoords;
    vec4 worldPos = vec4(Position, 1.0);
    positionCS = worldPos;
    //worldPos = model * worldPos;
    positionVS = vec4(worldPos.xyz, 1.0);
    worldPos = view * worldPos;
    
    worldPos = projection * worldPos;  
    //iPosition = worldPos.xyz;  
    rayPos = worldPos.xyz - camPos;
    //rayPos = worldPos.xyz - camPos;
    gl_Position = worldPos;  
    FS_IN_ClipPos = gl_Position;
}