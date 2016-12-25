#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in float passLinearDepth;

layout (location = 0) out float outDepth;
layout (location = 1) out vec2 outUv;
layout (location = 2) out vec4 outUvDerivatives;
layout (location = 3) out vec2 outNormal;

vec2 encodeNormal(vec3 n)
{
	float p = sqrt(n.z*8+8);
	return vec2(n.xy/p+0.5);
}

vec4 encodeDerivatives(vec4 deriv)
{
	return deriv*1e5;
}

void main()
{
	outDepth = passLinearDepth;

	vec2 uv = passUv.st;
	outUv = uv;
	outUvDerivatives = encodeDerivatives(vec4(dFdx(uv), dFdy(uv)));

	// normal encoding
	outNormal = encodeNormal(passNormal.xyz);
}