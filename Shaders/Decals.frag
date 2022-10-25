#version 300 es
precision mediump float;

layout (location = 0) out vec4 DecalTexture;
layout (location = 1) out vec4 DecalUVs;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
uniform vec2 iResolution;
uniform sampler2D iDepth;
uniform sampler2D iChannel0;
uniform sampler2D iRender;
uniform float iScale;
uniform float iFlip;
uniform float iBlend;

void main()
{

    vec2 uv = gl_FragCoord.xy / iResolution.xy;//gl_FragCoord.xy / iResolution.xy;
    vec4 renderResult = texture(iRender, uv);

    float depth = texture(iDepth, uv).r;

    if (gl_FragCoord.z > depth)
    {
        discard;
    }

    vec3 viewRay = vec3(uv, depth) * 2. - 1.;

    vec4 spaceToWorld = inverse(projection) * vec4(viewRay, 1.);
    vec4 worldToObject = inverse(view) * spaceToWorld;
    worldToObject = inverse(model) * worldToObject;
    worldToObject.xyz /= worldToObject.w;
    vec3 worldPos = worldToObject.xyz;

    if (worldPos.x < -1.0 || worldPos.x > 1.0 || worldPos.y < -1.0 || worldPos.y > 1.0 || worldPos.z < -1.0 || worldPos.z > 1.0)
    {
        //discard;
        worldPos *= 0.;
        DecalTexture.a = 0.;
    }

    if (iFlip == 1.0)
    {
        worldPos.y = 1. - worldPos.y;
    }

    vec2 uvs = (worldPos.xy + 0.5) * iScale;
    vec4 decalResult = texture(iChannel0, uvs);

    DecalTexture = vec4(mix(renderResult.rgb, decalResult.rgb, iBlend), renderResult.a);
    DecalUVs = vec4(uvs, 1., 1.);
}