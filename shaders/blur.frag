layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec3 outColor;

const float offsets[] = {0.0, 1.3846153846, 3.2307692308}; 
const float weights[] = {0.2270270270, 0.3162162162, 0.0702702703};

#if defined(BLUR_W)
	const vec2 dim = vec2(1,0);
#elif defined(BLUR_H)
	const vec2 dim = vec2(0,1);
#else
#error BLUR_W or BLUR_H not defined
#endif

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 invTexSize = 1.0/vec2(textureSize(tex, 0));
	vec3 sum = texelFetch(tex, coord, 0).rgb*weights[0];
	for (int i=1;i<3;++i)
	{
		sum += (
			textureLod(tex, (coord+dim*-offsets[i]+0.5)*invTexSize, 0).rgb+
			textureLod(tex, (coord+dim*+offsets[i]+0.5)*invTexSize, 0).rgb
			)*weights[i];
	}
	outColor = sum;
}