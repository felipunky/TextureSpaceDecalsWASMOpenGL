#version 300 es
precision mediump float;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;

out vec3 positions;
out vec3 normals;                                
out vec2 texCoords;                                    
                                 
void main()                                                
{                       
    positions = VertexPosition.xyz;
    texCoords = VertexTextureCoords;  
    vec4 position = /*projection * view * model * vec4(VertexPosition, 1.);//*/vec4(VertexTextureCoords.xy * 2. - 1., 0., 1.);
    gl_Position = position;           
}      