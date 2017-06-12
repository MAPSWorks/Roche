layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec3 outColor;

vec2 offsets[] = {
	vec2(-2,-2),vec2(0,-2),vec2(2,2),
	vec2(-1,-1),vec2(1,-1),
	vec2(-2,0),vec2(0,0),vec2(2,0),
	vec2(-1,1),vec2(1,1),
	vec2(-2,2),vec2(0,2),vec2(2,2)
};

float weights[] = {
	1,2,1,
	4,4,
	2,4,2,
	4,4,
	1,2,1
};

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 size = textureSize(tex, 0);
	vec2 middle = (coord*2+1)/size;
	vec2 offset = 1/size;

	vec3 sum = vec3(0);
	for (int i=0;i<13;++i)
	{
		sum += textureLod(tex, middle+offsets[i]*offset, 0).rgb*weights[i]/32.0;
	}
	outColor = sum;
}