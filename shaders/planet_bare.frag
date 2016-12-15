#version 450

// PIXEL ATTRIBUTES
layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in vec4 passPosition;
layout (location = 3) in vec4 passLpos;
layout (location = 0) out vec4 outColor;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	vec4 lightDir;
};

layout (binding = 2) uniform sampler2D diffuse;

void main()
{
	vec3 day = texture(diffuse, passUv.st).rgb;
	float light = 1;
	if (dot(lightDir,lightDir) > 0.1)
		light = dot(lightDir, passNormal);

	vec3 color = day*light;

	// Gamma-correction
	color = pow(color, vec3(invGamma));

	outColor = vec4(color, 1.0);
}

