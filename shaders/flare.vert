#version 330

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inUv;

layout(binding = 0, std140) uniform ubo
{
	vec4 color;
	vec2 pos;
	float size;
	float ratio;
};

layout(location = 0) out vec2 passUv;

void main(void)
{
	passUv = inUv;
	gl_Position = vec4(pos+inPosition.xy*size*vec2(1.0,ratio),0.0,1.0);
}