#version 300 es
precision mediump float;

in vec3 positions;                                                         
in vec2 texCoords;
in mat3 TBN;

// layout (location = 0) out vec3 gPosition;
// layout (location = 0) out vec4 gNormal;
layout (location = 0) out vec4 gAlbedo;
// layout (location = 3) out vec3 gMetallic;
// layout (location = 4) out vec3 gRoughness;
// layout (location = 5) out vec3 gAO;

uniform sampler2D BaseColor;
uniform sampler2D Normal;        
uniform sampler2D Roughness;
uniform sampler2D Metallic;
uniform sampler2D AmbientOcclussion; 
uniform float flipAlbedo;
uniform int iFlipper;

void main()
{        
    // and the diffuse per-fragment color
    vec2 texCoordsAlbedo = texCoords;
    if (flipAlbedo == 1.)
    {
        texCoordsAlbedo.y = 1. - texCoordsAlbedo.y;
    }
    // also store the per-fragment normals into the gbuffer
    //gNormal = vec4(normalize(texture(Normal, texCoords).xyz), 1.0);
    gAlbedo = texture(BaseColor, texCoordsAlbedo);
    //gMetallic = texture(Metallic, texCoords).rgb;
    //gRoughness = texture(Roughness, texCoords).rgb;
    //gAO = texture(AmbientOcclussion, texCoords).rgb;
}