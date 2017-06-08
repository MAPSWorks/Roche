layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;
layout(location = 2) in vec4 inNormal;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec4 passPosition;
layout(location = 1) out vec4 passUv;
layout(location = 2) out vec4 passNormal;
layout(location = 3) out vec4 passTangent;

void main(void)
{
	passPosition = inPosition;
	passUv = inUv;
	passNormal = inNormal;
	passTangent = inTangent;
}