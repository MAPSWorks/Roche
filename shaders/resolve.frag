#version 450

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
};

layout (binding = 1) uniform sampler2D hdr;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 color = texelFetch(hdr, ivec2(gl_FragCoord.xy), 0).rgb;
	outColor = vec4(pow(color, vec3(invGamma)), 1.0);
}