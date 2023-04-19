#version 300 es
precision mediump float;

layout (location = 0) in vec3 VertexPosition;  
layout (location = 1) in vec3 VertexNormals;                                 
layout (location = 2) in vec2 VertexTextureCoords;
layout (location = 3) in vec4 VertexTangents;
out vec3 positions;                             
out vec2 texCoords;     
out mat3 TBN;                               
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection;  
uniform mat4 decalProjector;    
uniform int iFlipper;      

mat2 rot(const in float a)
{
  vec2 sinCos = vec2(sin(a), cos(a));
  return mat2(sinCos.y, -sinCos.x,
              sinCos.x,  sinCos.y);
}

void main()                                                
{                

  texCoords = VertexTextureCoords;
  vec3 vertexNormals = VertexNormals; 
  if (iFlipper == 1)
  {
    texCoords.y = 1. - texCoords.y;
  }
  
  // vec3 T = normalize(vec3(model * vec4(VertexTangents.xyz, 0.0)));
  // vec3 N = normalize(vec3(model * vec4(VertexNormals,      0.0)));
  // vec3 B = cross(N, T) * VertexTangents.w;

  // TBN = mat3(T, B, N);
          
  vec4 worldPos = model * vec4(VertexPosition.xyz, 1.);    
  vec4 objPos = projection * view * worldPos;    
  positions = objPos.xyz;
  gl_Position = decalProjector * vec4(VertexPosition, 1.0);              
}                                                      