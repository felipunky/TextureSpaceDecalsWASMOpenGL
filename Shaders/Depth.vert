#version 300 es
precision mediump float;

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;                                   
uniform mat4 model;       
uniform mat4 view;
uniform mat4 projection;                                 
uniform mat4 decalProjector;                                   
void main()                                                
{                
    gl_Position = /*projection * view * model*/ decalProjector * vec4(VertexPosition, 1.0);              
}                                                      