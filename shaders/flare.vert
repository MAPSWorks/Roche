out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform flareDynamicUBO
{
	FlareUBO flareUBO;
};

layout (location = 0) out vec4 passUv;

void main()
{
	passUv = inUv;
	gl_Position = flareUBO.modelMat*inPosition;
}