#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;
layout (location = 2) in vec4 passPosition;

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

layout (binding = 2) uniform sampler2D atmo;

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

	float b = dot(viewer, view_dir);
	float c = dot(viewer,viewer) - pow(radius+atmos_height,2);
	float near = -b-sqrt(b*b-c);

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

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 norm_v = normalize(passPosition.xyz-planetPos.xyz);
	vec3 pp = norm_v*(radius+atmoHeight);
	vec3 viewer = viewPos.xyz-planetPos.xyz;
	vec3 view_dir = pp-viewer;
	float c = dot(normalize(view_dir),-lightDir.xyz);
	float cc = c*c;

	vec3 scat = in_scattering(viewer, view_dir, lightDir.xyz, radius, atmoHeight)
		* (K.xyz*rayleigh(cc) + K.www*mie(c,cc));

	float angle_view = dot(norm_v, -normalize(view_dir))*0.5+0.5;
	float opacity = exp(-texture(atmo, vec2(angle_view, 1.0)).g);
	outColor = vec4(scat, opacity);
}