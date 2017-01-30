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

layout (binding = 1) uniform sampler2DMS hdr;
layout (binding = 2) uniform sampler2D bloom;

layout (location = 0) out vec4 outColor;

void main()
{
	const int SAMPLES = textureSamples(hdr);
	const float SAMPLES_MUL = 1.0/float(SAMPLES);

	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 texCoord = coord/vec2(textureSize(hdr));

	vec3 sum = vec3(0);
	for (int i=0;i<SAMPLES;++i)
	{
		vec3 color = texelFetch(hdr, coord, i).rgb;
		// tonemap
		sum += color/(color+vec3(1));
	}
	vec3 finalColor = sum*SAMPLES_MUL+texture(bloom, texCoord).rgb;
	// Gamma correction
	outColor = vec4(pow(finalColor, vec3(invGamma)), 1.0);
}