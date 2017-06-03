layout (location = 0) in vec4 passUv;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform flareDynamicUBO
{
	FlareUBO flareUBO;
};

layout (binding = 2) uniform sampler1D intensityTex;
layout (binding = 3) uniform sampler2D linesTex;
layout (binding = 4) uniform sampler1D haloTex;

layout (location = 0) out vec4 outColor;

const float SIZE_DEGREES = 20.0;

void main()
{
	float intensity = texture(intensityTex, passUv.t).r;
	float lines = texture(linesTex, passUv.pq).r;
	float haloCoord = (passUv.t*SIZE_DEGREES-1.647)/2.422;
	vec3 halo = texture(haloTex, clamp(haloCoord,0,1)).rgb*0.2;
	float fade = pow(1-clamp(passUv.t,0,1),2.2);

	outColor = vec4(flareUBO.color.xyz*(vec3(intensity)+halo)*
		lines*flareUBO.brightness*fade,1.0);
}