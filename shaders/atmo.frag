layout (location = 0) in vec3 passPosition;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

layout (binding = 2) uniform sampler2D atmo;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 norm_v = normalize(passPosition-planetUBO.planetPos.xyz);
	vec3 localPos = norm_v*(planetUBO.radius+planetUBO.atmoHeight);
	vec3 view_dir = normalize(-passPosition);
	float c = dot(view_dir, planetUBO.lightDir.xyz);
	float cc = c*c;

	vec3 scat = in_scattering_atmo(localPos, view_dir, planetUBO.lightDir.xyz, 
		planetUBO.radius, planetUBO.atmoHeight,
		atmo, planetUBO.K)
		* (planetUBO.K.xyz*rayleigh(cc) + planetUBO.K.www*mie(c,cc));

	scat = clamp(scat, vec3(0),vec3(2));

	float angle_view = dot(norm_v, view_dir)*0.5+0.5;
	float opacity = exp(-texture(atmo, vec2(angle_view, 1.0)).g*0.5);
	outColor = vec4(scat, opacity);
}