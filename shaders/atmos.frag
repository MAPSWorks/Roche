#version 450

layout (location = 0) in vec4 passNormal;

layout (binding = 0, std140) uniform dynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	mat4 modelMat;
	vec4 viewPos;
	vec4 lightDir;
};

layout (binding = 1, std140) uniform staticUBO
{
	vec4 C_R;
	float E;
	float K_R;
	float K_M;
	float G_M;
	float planetRadius;
	float atmosHeight;
	float scaleHeight;
};


layout (binding = 3) uniform sampler2D lookupTable;

#define OUT_SAMPLES 5
#define IN_SAMPLES 5

#define PI 3.14159265359

layout (location = 0) out vec4 outColor;

float getNear(vec3 ray_origin, vec3 ray, float far)
{
	return 2*dot(ray,-viewPos.xyz) - far;
}

float raySphereFar(vec3 ori, vec3 ray, float radius)
{
	float b = dot(ori, ray);
	float c = dot(ori,ori) - radius*radius;
	return -b+sqrt(b*b-c);
}

float rayleigh(float cc)
{
	return 0.75 * (1.0 + cc);
}

float mie(float g, float c, float cc)
{
	float gg = g*g;
	float a = (1.0-gg)*(1.0+cc);
	float b = 1.0 + gg - 2.0*g*c;
	b*= sqrt(b);
	b*= 2.0+gg;

	return 1.5*a/b;
}

vec4 inScattering(vec3 viewer, vec3 fragPos, vec3 lightDir)
{
	vec3 viewDir = fragPos-viewer;
	float far = length(viewDir);
	viewDir /= far;

	float near = getNear(viewer, viewDir, far);

	float len = (far-near)/float(IN_SAMPLES);
	vec3 step = viewDir*len;

	vec3 p = viewer+viewDir*near;
	vec3 v = p+step*0.5;

	vec4 sum = vec4(0.0);
	for (int i=0;i<IN_SAMPLES;++i)
	{
		float t = raySphereFar(v,lightDir.xyz,planetRadius+atmosHeight);
		vec3 u = v+lightDir.xyz*t;

		float alt = (length(v)-planetRadius)/atmosHeight;
		vec3 norm_v = normalize(v);

		float angleView = acos(dot(norm_v, -viewDir))/PI;
		float angleLight = acos(dot(norm_v, lightDir))/PI;

		float n = texture(lookupTable, vec2(alt,angleView)).g +
							texture(lookupTable, vec2(alt,angleLight)).g;
		float dens = texture(lookupTable,vec2(alt,0.0)).r;
		sum += dens * exp(-n*(K_R*C_R+K_M));
		v += step;
	}

	sum *= len / scaleHeight;

	float c = dot(viewDir,-lightDir);
	float cc = c*c;

	vec4 color = sum * (K_R*C_R*rayleigh(cc) + K_M*mie(G_M, c,cc))*E;
	
	return color;
}

void main(void)
{
	outColor = inScattering(viewPos.xyz,normalize(passNormal.xyz)*(planetRadius+atmosHeight),lightDir.xyz);
}