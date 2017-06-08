layout(vertices = 4) out;

in int gl_InvocationID;

layout(location = 0) in vec4 inPosition[];
layout(location = 1) in vec4 inUv[];
layout(location = 2) in vec4 inNormal[];
layout(location = 3) in vec4 inTangent[];

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

layout(location = 0) out vec4 passPosition[];
layout(location = 1) out vec4 passUv[];
layout(location = 2) out vec4 passNormal[];
layout(location = 3) out vec4 passTangent[];

patch out float gl_TessLevelOuter[4];
patch out float gl_TessLevelInner[2];

mat4 getMatrix(PlanetUBO ubo)
{
#if defined(IS_ATMO)
	return ubo.atmoMat;
#elif defined(IS_FAR_RING)
	return ubo.ringFarMat;
#elif defined(IS_NEAR_RING)
	return ubo.ringNearMat;
#else
	return ubo.modelMat;
#endif
}

float edgeTessLevel(vec3 pos0, vec3 pos1)
{
	float tess = distance(pos0.xy/pos0.z, pos1.xy/pos1.z)*100;
	return clamp(tess, 1.0, 8.0);
}

void main()
{
	mat4 mMat = getMatrix(planetUBO);
	vec3 p0 = vec3(sceneUBO.viewMat*mMat*inPosition[0]);
	vec3 p1 = vec3(sceneUBO.viewMat*mMat*inPosition[1]);
	vec3 p2 = vec3(sceneUBO.viewMat*mMat*inPosition[2]);
	vec3 p3 = vec3(sceneUBO.viewMat*mMat*inPosition[3]);

	float tess0 = edgeTessLevel(p0, p2);
	float tess1 = edgeTessLevel(p0, p1);
	float tess2 = edgeTessLevel(p1, p3);
	float tess3 = edgeTessLevel(p2, p3);

	float innerTes0 = max(tess1, tess3);
	float innerTes1 = max(tess0, tess2);

	gl_TessLevelOuter[0] = tess0;
	gl_TessLevelOuter[1] = tess1;
	gl_TessLevelOuter[2] = tess2;
	gl_TessLevelOuter[3] = tess3;

	gl_TessLevelInner[0] = innerTes0;
	gl_TessLevelInner[1] = innerTes1;

	passPosition[gl_InvocationID] = inPosition[gl_InvocationID];
	passUv[gl_InvocationID] = inUv[gl_InvocationID];
	passNormal[gl_InvocationID] = inNormal[gl_InvocationID];
	passTangent[gl_InvocationID] = inTangent[gl_InvocationID];
}