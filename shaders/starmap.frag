layout(location = 0) in vec2 passUv;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1) uniform sampler2D starMap;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(starMap, passUv)*sceneUBO.starMapIntensity;
}