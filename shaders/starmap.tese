layout(quads, fractional_even_spacing) in;

in vec3 gl_TessCoord;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = 0) in vec3 inPosition[gl_MaxPatchVertices];
layout(location = 1) in vec2 inUv[gl_MaxPatchVertices];

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (location = 0) out vec2 passUv;

void main()
{
	passUv = lerp(inUv, gl_TessCoord);
	gl_Position = sceneUBO.projMat*sceneUBO.starMapMat*vec4(
		lerp(inPosition,gl_TessCoord), 1.0);
	gl_Position.z = 0.999*gl_Position.w;
}