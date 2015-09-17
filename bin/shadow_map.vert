#version 330

in vec4 in_position;
in vec2 in_uv;

uniform mat4 lightMat;
uniform mat4 modelMat;

out float pass_depth;
out vec2 pass_uv;

void main(void)
{
	gl_Position = lightMat*modelMat*in_position;
	pass_depth = gl_Position.z;
	pass_uv = in_uv;
}