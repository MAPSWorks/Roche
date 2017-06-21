layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in vec4 passPosition;
layout (location = 3) in vec4 passScattering;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

layout (binding = 2) uniform sampler2D diffuse;
layout (binding = 3) uniform sampler2D cloud;
layout (binding = 4) uniform sampler2D night;

layout (location = 0) out vec4 outColor;

#if defined(HAS_ATMO)
layout (binding = 5) uniform sampler2D atmo;
#endif

void main()
{
	vec3 day = texture(diffuse, passUv.st).rgb;

#if !defined(IS_STAR)
	float light = clamp(max(dot(planetUBO.lightDir.xyz, passNormal.xyz), sceneUBO.ambientColor),0,1);

	vec3 color = day*light;
#else
	vec3 color = day*planetUBO.starBrightness;
#endif

#if defined(HAS_ATMO)
	vec3 night = texture(night, passUv.st).rgb * planetUBO.nightIntensity;
	vec4 cloud = texture(cloud, passUv.st+vec2(planetUBO.cloudDisp, 0)).rrrr;

	night = night*clamp(-light*10+0.2,0,1)*(1-cloud.a);
	day = mix(day, cloud.rgb, cloud.a);

	color = day*light;

	vec3 norm_v = normalize(passPosition.xyz-planetUBO.planetPos.xyz);
	vec3 pp = norm_v*planetUBO.radius;
	vec3 viewer = sceneUBO.viewPos.xyz-planetUBO.planetPos.xyz;
	vec3 view_dir = pp-viewer;
	float c = dot(normalize(view_dir),-planetUBO.lightDir.xyz);
	float cc = c*c;

	vec3 scat = passScattering.rgb * (planetUBO.K.xyz*rayleigh(cc) + planetUBO.K.www*mie(c,cc));

	scat = clamp(scat, vec3(0),vec3(2));

	float angle_light = dot(norm_v, planetUBO.lightDir.xyz)*0.5+0.5;
	float angle_view = dot(norm_v, -normalize(view_dir))*0.5+0.5;
	color = color*
		exp(-texture(atmo, vec2(angle_view ,0)).g*(planetUBO.K.xyz+planetUBO.K.www))*
		exp(-texture(atmo, vec2(angle_light,0)).g*(planetUBO.K.xyz+planetUBO.K.www))+scat+night;
#endif

	outColor = vec4(color, 1.0);
} 