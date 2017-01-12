#version 450

layout (location = 0) in vec4 passUv;
layout (location = 1) in vec4 passNormal;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	mat4 projMat;
	mat4 viewMat;
	vec4 viewPos;
	float invGamma;
	float exposure;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	mat4 modelMat;
	vec4 lightDir;
	float albedo;
	float cloudDisp;
	float nightIntensity;
};

layout (binding = 2) uniform sampler2D diffuse;
layout (binding = 3) uniform sampler2D cloud;
layout (binding = 4) uniform sampler2D night;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 day = texture(diffuse, passUv.st).rgb * albedo;
	vec3 night = texture(night, passUv.st).rgb * nightIntensity;
	vec4 cloud = texture(cloud, passUv.st+vec2(cloudDisp, 0)) * vec4(vec3(albedo),1);

	float light = dot(lightDir.xyz, passNormal.xyz);

	night = night*clamp(-light*10+0.2,0,1)*(1-cloud.a);
	day = mix(day, cloud.rgb, cloud.a);

	vec3 color = day*clamp(light,0,1)+night;
	color *= exposure;

	outColor = vec4(color, 1.0);
} 