#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in float passLinearDepth;

layout (location = 1) out vec2 outUv;
layout (location = 2) out vec4 outUvDerivatives;

vec4 encodeDerivatives(vec4 deriv)
{
	return deriv*1e5;
}

void main()
{
	vec2 uv = passUv.st;
	outUv = uv;
	outUvDerivatives = encodeDerivatives(vec4(dFdx(uv), dFdy(uv)));
}