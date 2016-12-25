#version 450

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
};

layout (binding = 1) uniform sampler2DMS hdr;

layout (location = 0) out vec4 outColor;

void main()
{
	const int SAMPLES = textureSamples(hdr);
	const float SAMPLES_MUL = 1.0/float(SAMPLES);

	// Box filter
	vec3 color = vec3(0);
	for (int i=0;i<SAMPLES;++i)
	{
		vec3 s = texelFetch(hdr, ivec2(gl_FragCoord.xy), i).rgb;
		// Tonemapping
		color += s;
	}

	color *= SAMPLES_MUL;
	// Gamma correction
	outColor = vec4(pow(color, vec3(invGamma)), 1.0);
}