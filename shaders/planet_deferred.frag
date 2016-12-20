#version 450

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	vec4 lightDir;
	float cloudDisp;
};

layout (binding = 2) uniform sampler2D gbufferDepth;
layout (binding = 3) uniform sampler2D gbufferUv;
layout (binding = 4) uniform sampler2D gbufferUvDerivatives;
layout (binding = 5) uniform sampler2D gbufferNormal;

layout (binding = 6) uniform sampler2D diffuse;
layout (binding = 7) uniform sampler2D cloud;
layout (binding = 8) uniform sampler2D night;

layout (location = 0) out vec4 outColor;

vec3 decodeNormal(vec2 enc)
{
	vec2 fenc = enc*4-2;
	float f = dot(fenc,fenc);
	float g = sqrt(1-f/4);
	vec3 n;
	n.xy = fenc*g;
	n.z = 1-f/2;
	return n;
}

vec4 decodeDerivatives(vec4 deriv)
{
	return deriv/1e5;
}

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 uv = texelFetch(gbufferUv, coord, 0).st;
	vec4 uvDeriv = decodeDerivatives(texelFetch(gbufferUvDerivatives, coord, 0));

	vec3 day = textureGrad(diffuse, uv, uvDeriv.xy, uvDeriv.zw).rgb;
	vec4 cloud = textureGrad(cloud, uv+vec2(cloudDisp, 0), uvDeriv.xy, uvDeriv.zw);

	vec3 color = mix(day, cloud.rgb, cloud.a);

	vec3 normal = decodeNormal(texelFetch(gbufferNormal, coord, 0).xy);
	float light = dot(lightDir.xyz, normal);

	outColor = vec4(color*light, 1.0);
} 