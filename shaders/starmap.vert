layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec3 passPosition;
layout(location = 1) out vec2 passUv;

void main(void)
{
	passUv = inUv;
	passPosition = inPosition;
}