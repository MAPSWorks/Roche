#version 330
layout(location = 0)in vec4 in_position;
layout(location = 1)in vec2 in_uv;

uniform mat4 projMat;
uniform mat4 viewMat;
uniform mat4 modelMat;

out vec2 pass_uv;
out vec4 pass_position;
out vec3 pass_normal;
out mat4 pass_tbn;

void main(void)
{
	pass_uv = in_uv;
	vec3 normal = normalize(in_position.xyz);
	pass_normal = normalize(mat3(modelMat)*normal);
	pass_position = modelMat*in_position;
	vec3 tangent = normalize(vec3(normal.y + 0.000001, -normal.x, 0.0));
	mat4 tbn = mat4(1.0);
	tbn[0] = vec4(tangent,0.0);
	tbn[1] = vec4(cross(tangent, normal),1.0);
	tbn[2] = vec4(normal,0.0);
	pass_tbn = modelMat*tbn;
	gl_Position = projMat*viewMat*pass_position;
}