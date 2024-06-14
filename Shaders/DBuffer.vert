#version 300 es
precision highp float;

layout (location = 0) in vec3 VertexPosition;                              
 
uniform mat4 decalProjector;   

void main()                                                
{                
    gl_Position = decalProjector * vec4(VertexPosition, 1.0);              
}                                                      