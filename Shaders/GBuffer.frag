#version 300 es
precision mediump float;

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gAlbedo;
layout (location = 3) out vec3 gMetallic;
layout (location = 4) out vec3 gRoughness;
layout (location = 5) out vec3 gAO;

in vec3 positions;                                                         
in vec2 texCoords;
in mat3 TBN;
uniform sampler2D BaseColor;
uniform sampler2D Normal;        
uniform sampler2D Roughness;
uniform sampler2D Metallic;
uniform sampler2D AmbientOcclussion; 
uniform float flipAlbedo;
uniform int iFlipper;

void main()
{    
    // store the fragment position vector in the first gbuffer texture
    gPosition = positions;
    
    // and the diffuse per-fragment color
    vec2 texCoordsAlbedo = texCoords;
    if (flipAlbedo == 1.)
    {
        texCoordsAlbedo.y = 1. - texCoordsAlbedo.y;
        gPosition.y = 1. - gPosition.y;
    }
    // also store the per-fragment normals into the gbuffer
    gNormal = normalize(texture(Normal, texCoords).xyz);
    gAlbedo = texture(BaseColor, texCoordsAlbedo).rgb;
    //gMetallic = texture(Metallic, texCoords).rgb;
    //gRoughness = texture(Roughness, texCoords).rgb;
    //gAO = texture(AmbientOcclussion, texCoords).rgb;
}