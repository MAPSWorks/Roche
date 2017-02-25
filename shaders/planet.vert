#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;
layout(location = 2) in vec4 inNormal;
layout(location = 3) in vec4 inTangent;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float ambientColor;
	float invGamma;
	float exposure;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	mat4 atmoMat;
	vec4 planetPos;
	vec4 lightDir;
	vec4 K;
	float albedo;
	float cloudDisp;
	float nightIntensity;
	float radius;
	float atmoHeight;
};


#if defined(HAS_ATMO)
layout (binding = 5) uniform sampler2D atmo;

float ray_sphere_far(vec3 ori, vec3 ray, float radius)
{
	float b = dot(ori, ray);
	float c = dot(ori,ori) - radius*radius;
	return -b+sqrt(b*b-c);
}

float ray_sphere_near(vec3 ori, vec3 ray, float radius)
{
	float b = dot(ori, ray);
	float c = dot(ori,ori) - radius*radius;
	return -b-sqrt(b*b-c);
}

const int IN_SAMPLES = 50;

vec3 in_scattering(vec3 viewer, vec3 view_dir, vec3 light_dir, float radius, float atmos_height)
{
	float far = length(view_dir);
	view_dir = normalize(view_dir);

	float near = ray_sphere_near(viewer, view_dir, radius+atmos_height);

	float len = (far-near)/float(IN_SAMPLES);
	vec3 step = view_dir*len;

	vec3 p = viewer+view_dir*near;
	vec3 v = p+step*0.5;

	vec3 sum = vec3(0.0);
	for (int i=0;i<IN_SAMPLES;++i)
	{
		float alt = (length(v)-radius)/atmos_height;
		vec3 norm_v = normalize(v);

		float angle_view = dot(norm_v, -view_dir)*0.5+0.5;
		float angle_light = dot(norm_v, light_dir)*0.5+0.5;

		vec2 s = texture(atmo, vec2(angle_view,alt)).rg;

		float n = s.g + texture(atmo, vec2(angle_light,alt)).g;
		float dens = s.r;
		sum += dens * exp(-n*(K.xyz+K.www));
		v += step;
	}

	sum *= len / atmoHeight;

	return sum;
}

#endif

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;
layout (location = 2) out vec4 passPosition;
#if defined(HAS_ATMO)
layout (location = 3) out vec4 passScattering;
#endif

void main(void)
{
	passUv = inUv;
	mat4 mMat = modelMat;
#if defined(IS_ATMO)
	mMat = atmoMat;
#endif

	passNormal = normalize(viewMat*mMat*inNormal);
	passPosition = viewMat*mMat*inPosition;
	gl_Position = projMat*passPosition;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e10 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;

#if defined(HAS_ATMO)
	vec3 pp = passPosition.xyz-planetPos.xyz;
	vec3 viewer = viewPos.xyz-planetPos.xyz;
	vec3 view_dir = pp-viewer;
	vec3 scat = in_scattering(viewer, view_dir, lightDir.xyz, radius, atmoHeight);
	passScattering = vec4(scat, 0.0);
#endif
}