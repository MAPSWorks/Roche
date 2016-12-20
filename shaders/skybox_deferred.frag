#version 450

layout (binding = 2) uniform sampler2D gbufferDepth;
layout (binding = 3) uniform sampler2D gbufferUv;
layout (binding = 4) uniform sampler2D gbufferUvDerivatives;
layout (binding = 5) uniform sampler2D gbufferNormal;

layout (binding = 6) uniform sampler2D diffuse;

layout (location = 0) out vec4 outColor;

vec4 decodeDerivatives(vec4 deriv)
{
	return deriv/1e5;
}

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 uv = texelFetch(gbufferUv, coord, 0).st;
	vec4 uvDeriv = decodeDerivatives(texelFetch(gbufferUvDerivatives, coord, 0));

	outColor = vec4(textureGrad(diffuse, uv, uvDeriv.xy, uvDeriv.zw).rgb, 1.0);
}