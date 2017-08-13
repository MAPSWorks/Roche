out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout (binding = 0, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

layout (location = 0) out vec2 passUv;

void main()
{
	passUv = inUv;
	gl_Position = planetUBO.flareMat*vec4(inPosition, 1);
}