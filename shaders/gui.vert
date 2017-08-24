layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;

out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

layout (location = 0) out vec2 outUv;
layout (location = 1) out vec4 outColor;

void main()
{
	gl_Position = vec4(inPosition*2-vec2(1), 0, 1);
	outUv = inUv;
	outColor = inColor;
}