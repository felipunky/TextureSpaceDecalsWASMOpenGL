#version 300 es
precision mediump float;

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;
out vec3 positions;
out vec3 normals;                                
out vec2 TexCoords;                                    
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection;                                   
void main()                                                
{                
  vec4 p = vec4(VertexTextureCoords * 2. -1., 0., 1.);
  TexCoords = VertexTextureCoords;     
  mat3 normalMatrix = transpose(inverse(mat3(model)));
  normals = normalMatrix * VertexNormals;                         
  vec4 worldPos = model * vec4(VertexPosition.xyz, 1.0);
  vec4 objPos = projection * view * worldPos;      
  positions = (objPos).xyz;
  gl_Position = objPos;              
}                                                      