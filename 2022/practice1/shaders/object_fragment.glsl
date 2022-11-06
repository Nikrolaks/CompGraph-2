#version 330 core

uniform sampler2D texture_sampler;

in vec3 position;
in vec3 normal;
in vec2 texture_coord;

layout (location = 0) out vec4 out_color;

void main() {
	vec3 albedo = texture(texture_sampler, vec2(texture_coord.x, texture_coord.y)).rgb;
	out_color = vec4(albedo, 1.0);
}