#version 330

in vec4 pass_uv;

uniform sampler2D tex;

const float FXAA_SPAN_MAX = 8;
const float FXAA_REDUCE_MUL = 1.0/8.0;
const float FXAA_REDUCE_MIN = 1.0/128.0;

vec2 off = vec2(1.0/textureSize(tex,0).x, 1.0/textureSize(tex,0).y);

out vec4 out_color;

void main(void)
{
  vec3 rgbNW = textureLod(tex, pass_uv.zw,0).xyz;
  vec3 rgbNE = textureLodOffset(tex, pass_uv.zw, 0, ivec2(1,0)).xyz;
  vec3 rgbSW = textureLodOffset(tex, pass_uv.zw, 0, ivec2(0,1)).xyz;
  vec3 rgbSE = textureLodOffset(tex, pass_uv.zw, 0, ivec2(1,1)).xyz;
  vec3 rgbM  = textureLod(tex, pass_uv.xy,0).xyz;
  
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
              textureLod(tex, pass_uv.xy + dir * (1.0/3.0 - 0.5),0.0).xyz +
              textureLod(tex, pass_uv.xy + dir * (2.0/3.0 - 0.5),0.0).xyz);
  vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
              textureLod(tex, pass_uv.xy + dir * (0.0/3.0 - 0.5),0.0).xyz +
              textureLod(tex, pass_uv.xy + dir * (3.0/3.0 - 0.5),0.0).xyz);
  float lumaB = dot(rgbB, luma);

  out_color.rgb = mix(rgbB,rgbA, (lumaB < lumaMin) || (lumaB > lumaMax));
  out_color.a = 1.0;

  //out_color *= texture(tex, pass_uv);
}