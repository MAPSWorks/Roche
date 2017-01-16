#version 450

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float ambientColor;
	float invGamma;
	float exposure;
};

layout (binding = 1) uniform sampler2D hdr;
layout (binding = 2) uniform sampler2D bloomTex;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 color = texelFetch(hdr, ivec2(gl_FragCoord.xy), 0).rgb;
	vec2 texCoord = vec2(gl_FragCoord.xy)/vec2(textureSize(hdr, 0));
	vec3 bloom = texture(bloomTex, texCoord).rgb;
	color += bloom;
	// tonemap
	color = color/(color+vec3(1));
	// Gamma correction
	outColor = vec4(pow(color, vec3(invGamma)), 1.0);
}