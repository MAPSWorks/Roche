layout (binding = 0) uniform sampler2D texBlur;
layout (binding = 1) uniform sampler2D texHighpass;

layout (location = 0) out vec3 outColor;

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec2 texCoord = (vec2(coord)+vec2(0.5))/vec2(textureSize(texHighpass, 0));
	vec3 blur = textureLod(texBlur, texCoord, 0).rgb;
	outColor = texelFetch(texHighpass, coord, 0).rgb+blur;
}