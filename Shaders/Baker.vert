#version 300 es
precision mediump float;

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
    normals = VertexNormals;
    gl_Position = vec4(VertexPosition.xyz * 2. - 1., 1.);

    /*mat3 normalMatrix = transpose(inverse(mat3(model)));
    normals = normalMatrix * VertexNormals;                         
    vec4 worldPos = model * vec4(VertexPosition.xyz, 1.0);         
    gl_Position = projection * view * worldPos;*/              
}      