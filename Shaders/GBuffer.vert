#version 300 es
precision mediump float;

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;
out vec3 positions;
out vec3 normals;                                
out vec2 texCoords;                                    
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection;  
uniform mat4 decalProjector;                                 
void main()                                                
{                
  texCoords = VertexTextureCoords;     
  mat3 normalMatrix = transpose(inverse(mat3(model)));
  normals = normalMatrix * VertexNormals;                         
  vec4 worldPos = model * vec4(VertexPosition.xyz, 1.0);
  vec4 objPos = projection * view * worldPos;      
  positions = objPos.xyz;
  gl_Position = decalProjector * vec4(VertexPosition, 1.0);              
}                                                      