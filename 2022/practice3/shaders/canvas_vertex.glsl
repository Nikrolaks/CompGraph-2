#version 330 core

uniform mat4 view;
uniform float time;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;

out vec4 color;

void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    //color = vec4(0.7, 0.5, 0.0, 1.0);
    color = in_color;	
}