#version 330
in vec2 pass_uv;
in vec4 pass_position;

uniform sampler2D tex;
uniform vec4 ring_color;
uniform float minDist;

uniform sampler2DShadow shadow_map;
uniform mat4 lightMat;

out vec4 out_color;

void main(void)
{
    float dist = length(pass_uv);
    if (dist < minDist || dist > 1.0) discard;
    float matter = texture(tex, vec2((dist-minDist)/(1.0-minDist),0.0)).r;
    vec3 lightpos = (lightMat*pass_position).xyz*vec3(0.5) + vec3(0.5);
    float shadow = texture(shadow_map, lightpos);
	out_color = matter*ring_color*vec4(vec3(shadow),1.0);
}