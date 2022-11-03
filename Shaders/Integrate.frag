#version 300 es
precision mediump float;

in vec2 texCoords;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform vec2 iResolution;
uniform mat4 model;                                        
uniform mat4 view;                                         
uniform mat4 projection; 
out vec4 FragColor;

void main()
{    
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec4 tex = texture(iChannel0, texCoords);
    vec4 coords = vec4(texture(iChannel1, uv).xy, 0., 1.);
    coords = projection * view * model * coords;
    vec4 decal = texture(iChannel2, coords.xy);
    FragColor = vec4(mix(tex.rgb, decal.rgb, .5), 1.);
}