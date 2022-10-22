#version 300 es
precision mediump float;

in vec3 positions;
in vec3 normals;                                                          
in vec2 texCoords;
uniform sampler2D BaseColor;
uniform sampler2D Normal;        
uniform sampler2D Roughness;
uniform sampler2D Metallic;
uniform sampler2D AmbientOcclussion; 

out vec4 FragColor;

void main()
{    
    FragColor = texture(BaseColor, texCoords);
}