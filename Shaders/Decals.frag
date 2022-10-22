#version 300 es
precision mediump float;

uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
uniform vec2 iResolution;
uniform sampler2D iDepth;
uniform sampler2D iChannel0;
uniform vec3 camPos;
uniform vec3 camDir;
uniform float iTest;
uniform float iFlip;

in vec2 texCoords;
in vec3 iPosition;
in vec4 positionCS;
in vec4 positionVS;
in vec3 rayPos;
in vec4 FS_IN_ClipPos;

out vec4 FragColor;

const float FarClip = 200.0;

float near = 0.1; 

#define BIAS 0.001

void main()
{

    vec2 uv = gl_FragCoord.xy / iResolution.xy;//gl_FragCoord.xy / iResolution.xy;

    float depth = texture(iDepth, uv).r;

    if (gl_FragCoord.z > depth)
    {
        discard;
    }

    vec3 viewRay = vec3(uv, depth) * 2. - 1.;//*/vec3(positionVS.xyz) * (FarClip / -positionVS.z);
    vec3 viewPosition = viewRay;// * depth;

    vec4 spaceToWorld = inverse(projection) * vec4(viewPosition, 1.);
    vec4 worldToObject = inverse(view) * spaceToWorld;
    worldToObject = inverse(model) * worldToObject;
    worldToObject.xyz /= worldToObject.w;
    vec3 worldPos = worldToObject.xyz;

    if (worldPos.x < -1.0 || worldPos.x > 1.0 || worldPos.y < -1.0 || worldPos.y > 1.0 || worldPos.z < -1.0 || worldPos.z > 1.0)
    {
        discard;
    }

    if (iFlip == 1.0)
    {
        worldPos.y = 1. - worldPos.y;
    }

    FragColor = texture(iChannel0, (worldPos.xy + 0.5) * iTest);//vec4(1, 0, 0, 1);
}