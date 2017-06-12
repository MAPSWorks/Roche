layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1) uniform sampler2DMS hdr;

layout (location = 0) out vec3 outColor;

float bloomCurve(vec3 hdr)
{
	float lum = dot(vec3(0.2126,0.7152,0.0722), hdr);
	return (lum>1)?(lum-1)*1.4+0.2:lum*0.2;
}

void main()
{
	const int SAMPLES = textureSamples(hdr);
	const float SAMPLES_MUL = 1.0/float(SAMPLES);

	const ivec2 coord = ivec2(gl_FragCoord.xy);

	vec3 sum = vec3(0);
	for (int i=0;i<SAMPLES;++i)
	{
		sum += texelFetch(hdr, coord, i).rgb;
	}
	const vec3 hdr = sum*SAMPLES_MUL*sceneUBO.exposure;
	outColor = bloomCurve(hdr)*hdr;
}