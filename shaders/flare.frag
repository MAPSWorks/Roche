#version 450

layout(location = 0) in vec2 passUv;

layout(binding = 0, std140) uniform ubo
{
	vec4 color;
	vec2 pos;
	float size;
	float ratio;
};

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main(void)
{
	outColor = texture(tex, passUv)*color;
}