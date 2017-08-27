out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

const vec2 pos[3] = {vec2(-1,-1),vec2(3,-1),vec2(-1,3)};

void main()
{
	gl_Position = vec4(pos[gl_VertexID],0,1);
}