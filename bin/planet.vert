#version 330
layout(location = 0)in vec4 in_position;
layout(location = 1)in vec2 in_uv;

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 modelMat;

out vec2 pass_uv;
out vec4 pass_position;
out vec3 pass_normal;

void main(void)
{
	pass_uv = in_uv;
	vec3 normal = normalize(in_position.xyz);
	pass_normal = normalize(mat3(modelMat)*normal);
	pass_position = modelMat*in_position;
	gl_Position = projMat*viewMat*pass_position;
}