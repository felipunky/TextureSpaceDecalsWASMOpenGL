#version 300 es
precision mediump float;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoords;

out vec2 texCoords;

void main()
{
    texCoords = TexCoords;
    vec4 worldPos = vec4(Position, 1.0);
    gl_Position = worldPos;
}