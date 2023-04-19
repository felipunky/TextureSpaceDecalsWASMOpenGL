#version 300 es
precision mediump float;

//in vec2 TexCoords;
in vec3 positions;                               
in vec2 TexCoords;
in mat3 TBN; 

uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMetallic;
uniform sampler2D gRoughness;
uniform sampler2D gAO;

out vec4 FragColor;

uniform vec3 viewPos;
uniform float iTime;
uniform float iFlipAlbedo;
uniform int iFlipper;
uniform int iNormals;

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap(const in vec2 texCoords)
{
    vec3 tangentNormal = texture(gNormal, texCoords).xyz * 2.0 - 1.0;
    return normalize(tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------

void main()
{             
    vec3 FragPos    = positions;//texture(gPosition, TexCoords).xyz;
    vec2 texCoordsAlbedo = TexCoords;
    if (iFlipAlbedo == 1.0)
    {
        texCoordsAlbedo.y = 1. - texCoordsAlbedo.y;
    }
    vec3 albedo = texture(gAlbedo, texCoordsAlbedo).rgb;

    if (iNormals == 1)
    {

        vec3 light = vec3(2.+sin(iTime), 1.0+cos(iTime), -6.);
        vec3 L = normalize(light - FragPos);

        if (iFlipper == 1)
        {
            texCoordsAlbedo.y = 1. - texCoordsAlbedo.y;
        }

        vec3 N = getNormalFromMap(texCoordsAlbedo);
        N = normalize(TBN * N);

        vec3 col = albedo * max(0., dot(L, -N));

        FragColor = vec4(pow(col, vec3(2.2)), 1.0);

    }
    else
    {
        FragColor = vec4(pow(albedo, vec3(2.2)), 1.0);
    }

    // float metallic  = 0.0;//texture(gMetallic, TexCoords).r;
    // float roughness = texture(gRoughness, TexCoords).r;
    // float ao        = texture(gAO, TexCoords).r;

    // vec3 V = normalize(viewPos - FragPos);

    // // calculate reflectance at normal incidence; if diaz-electric (like plastic) use F0 
    // // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    // vec3 F0 = vec3(0.04); 
    // F0 = mix(F0, albedo, metallic);

    // // reflectance equation
    // vec3 Lo = vec3(0.0);
    // for(int i = 0; i < 1; ++i) 
    // {
    //     // calculate per-light radiance
    //     vec3 light = vec3(3.*sin(iTime), 10.0, -5.*cos(iTime));
    //     vec3 L = normalize(light - FragPos);//normalize(lightPositions[i] - WorldPos);
    //     vec3 H = normalize(V + L);
    //     float distance = length(light - FragPos);
    //     float attenuation = 1.0 / (distance * distance);
    //     vec3 radiance = vec3(300);//lightColors[i] * attenuation;

    //     // Cook-Torrance BRDF
    //     float NDF = DistributionGGX(N, H, roughness);   
    //     float G   = GeometrySmith(N, V, L, roughness);      
    //     vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
    //     vec3 numerator    = NDF * G * F; 
    //     float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    //     vec3 specular = numerator / denominator;
        
    //     // kS is equal to Fresnel
    //     vec3 kS = F;
    //     // for energy conservation, the diffuse and specular light can't
    //     // be above 1.0 (unless the surface emits light); to preserve this
    //     // relationship the diffuse component (kD) should equal 1.0 - kS.
    //     vec3 kD = vec3(1.0) - kS;
    //     // multiply kD by the inverse metalness such that only non-metals 
    //     // have diffuse lighting, or a linear blend if partly metal (pure metals
    //     // have no diffuse light).
    //     kD *= 1.0 - metallic;	  

    //     // scale light by NdotL
    //     float NdotL = max(dot(N, L), 0.0);        

    //     // add to outgoing radiance Lo
    //     Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    // }   
    
    // // ambient lighting (note that the next IBL tutorial will replace 
    // // this ambient lighting with environment lighting).
    // vec3 ambient = vec3(0.03) * albedo * ao;
    
    // vec3 color = ambient + Lo;

    // // HDR tonemapping
    // color = color / (color + vec3(1.0));
    // // gamma correct
    // color = pow(color, vec3(1.0/2.2)); 
    // FragColor = vec4(albedo, 1.0);
}