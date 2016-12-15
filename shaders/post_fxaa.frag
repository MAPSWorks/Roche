#version 330

in vec4 pass_uv;

uniform sampler2D tex;

const float FXAA_SPAN_MAX = 8;
const float FXAA_REDUCE_MUL = 1.0/8.0;
const float FXAA_REDUCE_MIN = 1.0/128.0;

uniform vec2 off;

#define fxaa_tex(tex, uv) min(textureLod(tex, uv, 0).xyz,1.0)
#define fxaa_texOffset(tex, uv, offset) min(textureLodOffset(tex, uv, 0, offset).xyz,1.0)

out vec4 out_color;

void main(void)
{
	vec3 rgbNW = fxaa_tex(tex, pass_uv.zw);
	vec3 rgbNE = fxaa_texOffset(tex, pass_uv.zw,ivec2(1,0));
	vec3 rgbSW = fxaa_texOffset(tex, pass_uv.zw,ivec2(0,1));
	vec3 rgbSE = fxaa_texOffset(tex, pass_uv.zw,ivec2(1,1));
	vec3 rgbM  = fxaa_tex(tex, pass_uv.xy);
	
	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot( rgbM, luma);
	
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
	
	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
		
	float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
	
	dir = min(vec2(FXAA_SPAN_MAX,  FXAA_SPAN_MAX), 
				max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * off;
		
	vec3 rgbA = (1.0/2.0) * (
							fxaa_tex(tex, pass_uv.xy + dir * (1.0/3.0 - 0.5)) +
							fxaa_tex(tex, pass_uv.xy + dir * (2.0/3.0 - 0.5)));
	vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
							fxaa_tex(tex, pass_uv.xy + dir * (0.0/3.0 - 0.5)) +
							fxaa_tex(tex, pass_uv.xy + dir * (3.0/3.0 - 0.5)));
	float lumaB = dot(rgbB, luma);

	if ((lumaB < lumaMin) || (lumaB > lumaMax))
	{
		out_color.rgb = rgbA;
	}
	else
	{
		out_color.rgb = rgbB;
	}
	out_color.a = 1.0;
}