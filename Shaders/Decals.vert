#version 300 es
precision mediump float;

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;
out vec3 WorldPos;  
out vec2 TexCoords;                                
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection;  
uniform mat4 decalProjector; 
uniform float iFlipAlbedo;     

void main()                                                
{                                     
  vec4 objPos = vec4(VertexPosition.xyz, 1.0);      
  WorldPos = (decalProjector * objPos).xyz * 0.5 + 0.5;
  TexCoords = VertexTextureCoords;
  vec4 Position = vec4(VertexTextureCoords, 0., 1.);
  if (iFlipAlbedo == 1.)
  {
    Position.y = 1. - Position.y;
    TexCoords.y = 1. - TexCoords.y;
  }
  Position.xy = Position.xy * 2. - 1.;
  gl_Position = Position;              
}                                                      