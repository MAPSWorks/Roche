layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 passPosition;
layout(location = 1) out vec2 passUv;
layout(location = 2) out vec3 passNormal;

void main(void)
{
	passPosition = inPosition;
	passUv = inUv;
	passNormal = inNormal;
}