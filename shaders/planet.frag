#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in vec4 passPosition;
#if defined(HAS_ATMO)
layout (location = 3) in vec4 passScattering;
#endif

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float ambientColor;
	float exposure;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	mat4 atmoMat;
	mat4 ringFarMat;
	mat4 ringNearMat;
	vec4 planetPos;
	vec4 lightDir;
	vec4 K;
	float albedo;
	float cloudDisp;
	float nightIntensity;
	float radius;
	float atmoHeight;
};
layout (binding = 2) uniform sampler2D diffuse;
layout (binding = 3) uniform sampler2D cloud;
layout (binding = 4) uniform sampler2D night;

layout (location = 0) out vec4 outColor;

#if defined(HAS_ATMO)

layout (binding = 5) uniform sampler2D atmo;

float rayleigh(float cc)
{
	return 0.75 * (1.0 + cc);
}

const float G_M = -0.85;
const float G_M2 = G_M*G_M;

float mie(float c, float cc)
{
	float a = (1.0-G_M2)*(1.0+cc);
	float b = 1.0 + G_M2 - 2.0*G_M*c;
	b*= sqrt(b);
	b*= 2.0+G_M2;

	return 1.5*a/b;
}
#endif

void main()
{
	vec3 day = texture(diffuse, passUv.st).rgb;

#if !defined(IS_STAR)
	float light = clamp(max(dot(lightDir.xyz, passNormal.xyz), ambientColor),0,1);

	vec3 color = day*light;
#else
	vec3 color = day*albedo;
#endif

#if defined(HAS_ATMO)
	vec3 night = texture(night, passUv.st).rgb * nightIntensity;
	vec4 cloud = texture(cloud, passUv.st+vec2(cloudDisp, 0));

	night = night*clamp(-light*10+0.2,0,1)*(1-cloud.a);
	day = mix(day, cloud.rgb, cloud.a);

	color = day*light;

	vec3 norm_v = normalize(passPosition.xyz-planetPos.xyz);
	vec3 pp = norm_v*radius;
	vec3 viewer = viewPos.xyz-planetPos.xyz;
	vec3 view_dir = pp-viewer;
	float c = dot(normalize(view_dir),-lightDir.xyz);
	float cc = c*c;

	vec3 scat = passScattering.rgb * (K.xyz*rayleigh(cc) + K.www*mie(c,cc));

	scat = clamp(scat, vec3(0),vec3(2));

	float angle_light = dot(norm_v, lightDir.xyz)*0.5+0.5;
	float angle_view = dot(norm_v, -normalize(view_dir))*0.5+0.5;
	color = color*
		exp(-texture(atmo, vec2(angle_view ,0)).g*(K.xyz+K.www))*
		exp(-texture(atmo, vec2(angle_light,0)).g*(K.xyz+K.www))+scat+night;
#endif

#if !defined(IS_STAR)
	color *= exposure;
#endif

	outColor = vec4(color, 1.0);
} 