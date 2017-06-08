layout(vertices = 4) out;

in int gl_InvocationID;

layout(location = 0) in vec4 inPosition[];
layout(location = 1) in vec4 inUv[];
layout(location = 2) in vec4 inNormal[];
layout(location = 3) in vec4 inTangent[];

layout(location = 0) out vec4 passPosition[];
layout(location = 1) out vec4 passUv[];
layout(location = 2) out vec4 passNormal[];
layout(location = 3) out vec4 passTangent[];

patch out float gl_TessLevelOuter[4];
patch out float gl_TessLevelInner[2];

void main()
{
	gl_TessLevelOuter[0] = 4.0;
	gl_TessLevelOuter[1] = 4.0;
	gl_TessLevelOuter[2] = 4.0;
	gl_TessLevelOuter[3] = 4.0;

	gl_TessLevelInner[0] = 4.0;
	gl_TessLevelInner[1] = 4.0;

	passPosition[gl_InvocationID] = inPosition[gl_InvocationID];
	passUv[gl_InvocationID] = inUv[gl_InvocationID];
	passNormal[gl_InvocationID] = inNormal[gl_InvocationID];
	passTangent[gl_InvocationID] = inTangent[gl_InvocationID];
}