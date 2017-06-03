out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

layout(location = 0) in vec4 inPosition;

void main()
{
	gl_Position = inPosition;
}