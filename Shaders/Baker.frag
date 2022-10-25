#version 300 es
precision mediump float;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 

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
    // vec4 uv = model * vec4(texCoords, 1., 1.);     
    // uv = projection * view * uv;  
    vec2 uv = texture(Normal, texCoords).xy;
    FragColor = texture(BaseColor, uv);
}