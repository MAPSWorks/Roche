layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in vec4 passPosition;
layout (location = 3) in vec4 passLocalPosition;
layout (location = 4) in vec4 passScattering;

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
layout (binding = 5) uniform sampler2D specular;

layout (location = 0) out vec4 outColor;

#if defined(HAS_ATMO)
layout (binding = 6) uniform sampler2D atmo;
#endif

#if defined(HAS_RING)
layout (binding = 7) uniform sampler1D ringOcclusion;
#endif

void main()
{
	vec3 day = texture(diffuse, passUv.st).rgb;

	// Light calculations
	vec3 normal = normalize(passNormal.xyz);
	vec3 lightDir = planetUBO.lightDir.xyz;
	vec3 planetPos = planetUBO.planetPos.xyz;
	vec3 pp = normalize(passLocalPosition.xyz)*planetUBO.radius;
	vec3 viewDir = normalize(pp+planetPos);

	float lambert = clamp(max(dot(lightDir, normal), sceneUBO.ambientColor),0,1);

	// Specular calculation
	float spec = texture(specular, passUv.st).r;
	vec3 H = normalize(lightDir - viewDir);
	float NdotH = clamp(dot(normal, H), 0, 1);

	vec3 specColor = mix(planetUBO.mask0ColorHardness.rgb, planetUBO.mask1ColorHardness.rgb, spec);
	float hardness0 = planetUBO.mask0ColorHardness.w;
	float hardness1 = planetUBO.mask1ColorHardness.w;
	float specIntensity0 = hardness0 < 1 ? 0 : pow(NdotH, hardness0);
	float specIntensity1 = hardness1 < 1 ? 0 : pow(NdotH, hardness1);
	float specIntensity = mix(specIntensity0, specIntensity1, spec);

	// Clouds & night
	float nightTex = texture(night, passUv.st).r * planetUBO.nightIntensity;
	float cloudTex = texture(cloud, passUv.st+vec2(planetUBO.cloudDisp, 0)).r;

	vec3 nightFinal = vec3(nightTex*clamp(-lambert*10+0.2,0,1)*(1-cloudTex));
	float k = mix(specIntensity, 0, cloudTex);
	vec3 dayWithClouds = mix(day, vec3(cloudTex), cloudTex);

#if !defined(IS_STAR)
	vec3 color = dayWithClouds*lambert*(1-k) + k*specColor;
#else
	vec3 color = day*planetUBO.starBrightness;
#endif

#if defined(HAS_ATMO)
	float c = dot(viewDir,-lightDir);
	float cc = c*c;

	vec3 scat = passScattering.rgb * (planetUBO.K.xyz*rayleigh(cc) + planetUBO.K.www*mie(c,cc));

	scat = clamp(scat, vec3(0),vec3(2));

	float angleLight = dot(normal, lightDir)*0.5+0.5;
	float angleView = dot(normal, -viewDir)*0.5+0.5;
	color = color*
		exp(-texture(atmo, vec2(angleView ,0)).g*(planetUBO.K.xyz+planetUBO.K.www))*
		exp(-texture(atmo, vec2(angleLight,0)).g*(planetUBO.K.xyz+planetUBO.K.www))+scat;
#endif

#if defined(HAS_RING)
	// Ray-plane test
	vec3 rayOrigin = pp;
	vec3 rayDir = lightDir;
	vec3 ringNormal = planetUBO.ringNormal.xyz;

	float t = -dot(rayOrigin, ringNormal)/dot(rayDir, ringNormal);
	float dist = length(rayOrigin+t*rayDir);
	float texOffset = (dist-planetUBO.ringInner)/(planetUBO.ringOuter-planetUBO.ringInner);

	vec4 ring = texture(ringOcclusion, texOffset);
	float mul = t>=0 && texOffset > 0 && texOffset < 1 ? ring.a : 1.0;
	color *= mul;
#endif

	outColor = vec4(color+nightFinal, 1.0);
} 