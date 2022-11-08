#version 300 es
precision mediump float;

out vec4 FragColor;

//layout (location = 0) out vec4 FragColor;
//layout (location = 1) out vec4 DecalCoords;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
uniform mat4 decalProjector;

uniform vec2 iResolution;
//uniform sampler2D iDepth;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
//uniform sampler2D iRender;
uniform float iScale;
uniform float iFlip;
uniform float iBlend;

in vec2 TexCoords;
in vec3 WorldPos;

#define BIAS 0.001

bool CheckBox(vec3 uv)
{
    return (uv.x > 1.0 || uv.x < 0.0 || uv.y > 1.0 || uv.y < 0.0 || uv.z > 1.0 || uv.z < 0.0);
}

void main()
{

    //vec4 decalPos = decalProjector * vec4(WorldPos, 1.0);
    vec3 decalUV = WorldPos;//decalPos.xyz * 0.5 + 0.5;
    float clip = 1.0;
    if (CheckBox(decalUV))
    {
        //FragColor = texture(iChannel1, TexCoords);
        //FragColor.a = 0.0;
        //discard;
        decalUV *= 0.;
        clip = 0.;
    }

    if (iFlip == 1.0)
    {
        decalUV.y = 1. - decalUV.y;
    }

    vec4 projectedDecal = texture(iChannel0, decalUV.xy);
    vec4 albedoMap      = texture(iChannel1, TexCoords);

    // Sample the decal texture using the Decal UVs.
    projectedDecal = mix(projectedDecal, albedoMap, iBlend) * clip;
    albedoMap      *= 1. - clip;
    FragColor = projectedDecal + albedoMap;//mix(albedoMap, projectedDecal, iBlend);

    // vec2 uv = gl_FragCoord.xy / iResolution.xy;
    // vec4 renderResult = texture(iRender, uv);

    // float depth = texture(iDepth, uv).r;

    // // if (gl_FragCoord.z > depth)
    // // {
    // //     discard;
    // // }

    // vec3 viewRay = vec3(uv, depth) * 2. - 1.;

    // vec4 spaceToWorld = inverse(projection) * vec4(viewRay, 1.);
    // vec4 worldToObject = inverse(view) * spaceToWorld;
    // worldToObject = inverse(model) * worldToObject;
    // worldToObject.xyz /= worldToObject.w;
    // vec3 worldPos = worldToObject.xyz;
    // FragColor.a = 1.0;

    // if (worldPos.x < -1.0 || worldPos.x > 1.0 || worldPos.y < -1.0 || worldPos.y > 1.0 || worldPos.z < -1.0 || worldPos.z > 1.0 || gl_FragCoord.z > depth)
    // {
    //     //discard;
    //     worldPos *= 0.;
    //     FragColor.a = 0.;
    // }

    // if (iFlip == 1.0)
    // {
    //     worldPos.y = 1. - worldPos.y;
    // }

    // vec2 uvs = (worldPos.xy + 0.5) * iScale;

    // DecalCoords = vec4(uvs, 1.0, 1.0);

    // vec4 decalResult = texture(iChannel0, uvs) * FragColor.a;

    // FragColor = vec4(mix(renderResult.rgb, decalResult.rgb, iBlend), renderResult.a);
}