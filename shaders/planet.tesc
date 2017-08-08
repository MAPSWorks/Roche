layout(vertices = 4) out;

in int gl_InvocationID;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec2 inUv[];
layout(location = 2) in vec3 inNormal[];

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

layout(location = 0) out vec3 passPosition[];
layout(location = 1) out vec2 passUv[];
layout(location = 2) out vec3 passNormal[];

patch out float gl_TessLevelOuter[4];
patch out float gl_TessLevelInner[2];

float edgeTessLevel(vec3 pos0, vec3 pos1)
{
	float d = 1.0/max(pos0.z, pos1.z);
	float tess = distance(pos0.xy*d, pos1.xy*d)*120;
	return clamp(tess, 1.0, 16.0);
}

void main()
{
	mat4 mMat = sceneUBO.projMat*sceneUBO.viewMat*getMatrix(planetUBO);
	vec3 p0 = vec3(mMat*vec4(inPosition[0],1));
	vec3 p1 = vec3(mMat*vec4(inPosition[1],1));
	vec3 p2 = vec3(mMat*vec4(inPosition[2],1));
	vec3 p3 = vec3(mMat*vec4(inPosition[3],1));

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
}