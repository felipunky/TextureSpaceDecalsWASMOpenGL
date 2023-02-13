#version 300 es
precision mediump float;

out vec4 FragColor;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
uniform mat4 decalProjector;

uniform vec2 iResolution;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iDepth;
uniform float iScale;
uniform float iFlip;
uniform float iBlend;

in vec2 TexCoords;
in vec3 WorldPos;

#define BIAS 1e-5

bool CheckBox(vec3 uv)
{
    return (uv.x > 1.0 || uv.x < 0.0 || uv.y > 1.0 || uv.y < 0.0 || uv.z > 1.0 || uv.z < 0.0);
}

void main()
{
    vec3 decalUV = WorldPos;
    vec2 texCoords = TexCoords;
    float clip = 1.0;
    if (CheckBox(decalUV))
    {
        decalUV *= 0.;
        clip = 0.;
    }
    float depth = texture(iDepth, decalUV.xy).r;
    if ((decalUV.z - BIAS) > depth)
    {
        decalUV *= 0.;
        clip = 0.;
    }

    if (iFlip == 1.0)
    {
        decalUV.y = 1. - decalUV.y;
    }

    decalUV *= iScale;

    vec4 projectedDecal = texture(iChannel0, decalUV.xy);
    vec4 albedoMap      = texture(iChannel1, texCoords);

    // Sample the decal texture using the Decal UVs.
    projectedDecal = mix(projectedDecal, albedoMap, iBlend) * clip;
    albedoMap      *= 1. - clip;
    FragColor = projectedDecal + albedoMap;
}