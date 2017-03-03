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

layout (binding = 2) uniform sampler1D tex1;
layout (binding = 3) uniform sampler1D tex2;

layout (location = 0) out vec4 outColor;

void main(void)
{
	float len = length(passUv);

	// Ring color & transparency
	vec4 color = texture(tex2, len-1.0);

	vec3 t1 = texture(tex1, len-1.0).rgb;
	// Determine if back scattering or forward scattering
	float backscat = lightDir.z*0.5+0.5;

	// Color with scattering
	vec3 scat = color.rgb*mix(t1.g,t1.r,backscat);

	// Shadow
	vec3 pos = passPosition.xyz-planetPos.xyz;	
	float b = dot(pos, lightDir.xyz);
	float c = dot(pos,pos) - radius*radius;
	float d = b*b-c;
	// Shadow edge antialiasing
	float saf = fwidth(d);
	// mix lit and unlit
	float shadow = b<0?smoothstep(-saf,0,d):0.0;
	vec3 unlit = color.rgb*t1.b*ambientColor;
	vec4 shadowed = vec4(mix(scat,unlit,shadow),color.a);

	// Edge antialiasing
	float aaf = fwidth(abs(len-1.5));
	float alpha = smoothstep(0.5-aaf,0.5,abs(len-1.5));
	outColor = mix(vec4(0,0,0,1), shadowed, 1-alpha);
	outColor.rgb *= (1-outColor.a)*exposure;

}