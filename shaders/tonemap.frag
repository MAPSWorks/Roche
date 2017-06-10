layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};
layout (binding = 1) uniform sampler2DMS hdr;
layout (binding = 2) uniform sampler2D bloom;

layout (location = 0) out vec4 outColor;

vec3 reinhard(vec3 color)
{
	return color/(vec3(1)+color);
}

void main()
{
	const int SAMPLES = textureSamples(hdr);
	const float SAMPLES_MUL = 1.0/float(SAMPLES);

	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 texCoord = coord/vec2(textureSize(hdr));

	vec3 sum = vec3(0);
	for (int i=0;i<SAMPLES;++i)
	{
		vec3 color = texelFetch(hdr, coord, i).rgb*sceneUBO.exposure;
		// tonemap
		sum += reinhard(color);
	}
	vec3 bloom = texture(bloom, texCoord).rgb*sceneUBO.exposure;
	vec3 finalColor = sum*SAMPLES_MUL+bloom;
	outColor = vec4(finalColor, 1.0);
}