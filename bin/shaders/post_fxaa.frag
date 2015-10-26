#version 330

in vec2 pass_uv;

uniform sampler2D tex;

#define FXAA_EDGE_THRESHOLD (1.0/8.0)
#define FXAA_EDGE_THRESHOLD_MIN (1.0/16.0)
#define FXAA_SUBPIX (1)
#define FXAA_SUBPIX_TRIM (1.0/8.0)
#define FXAA_SUBPIX_CAP (3.0/4.0)

vec2 p = 1.0/textureSize(tex,0);

vec2 nbrs[9] = vec2[](
  vec2(0,0),
  vec2(0,-1),
  vec2(-1,0),
  vec2(1,0),
  vec2(0,1),
  vec2(-1,-1),
  vec2(1,-1),
  vec2(-1,1),
  vec2(1,1));

out vec4 out_color;

float fxaa_luma(vec3 c)
{
  return c.g * (0.587/0.299) + c.x;
}

void main(void)
{
  vec3 rgb_m = texture(tex, pass_uv+nbrs[0]*p).rgb;
  vec3 rgb_n = texture(tex, pass_uv+nbrs[1]*p).rgb;
  vec3 rgb_w = texture(tex, pass_uv+nbrs[2]*p).rgb;
  vec3 rgb_e = texture(tex, pass_uv+nbrs[3]*p).rgb;
  vec3 rgb_s = texture(tex, pass_uv+nbrs[4]*p).rgb;

  float luma_m = fxaa_luma(rgb_m);
  float luma_n = fxaa_luma(rgb_n);
  float luma_w = fxaa_luma(rgb_w);
  float luma_e = fxaa_luma(rgb_e);
  float luma_s = fxaa_luma(rgb_s);

  float range_min = min(luma_m,min(min(luma_n,luma_w),min(luma_s,luma_e)));
  float range_max = max(luma_m,max(max(luma_n,luma_w),max(luma_s,luma_e)));
  float range = range_max-range_min;

  if (range < max(FXAA_EDGE_THRESHOLD_MIN, range_max*FXAA_EDGE_THRESHOLD))
  {
    // fxaafilterreturn(rgb_m)
  }
  float luma_l = (luma_m+luma_w+luma_e+luma_s)*0.25;
  float range_l = abs(luma_l - luma_m)
  float blend_l = max(0.0, (range_l / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
  blend_l = min(FXAA_SUBPIX_CAP, blend_l);
  vec3 rgb_l = rgb_n+rgb_w+rgb_m+rgb_e+rgb_s;
  vec3 rgb_nw = texture(tex, pass_uv+nbrs[5]*p).rgb;
  vec3 rgb_ne = texture(tex, pass_uv+nbrs[6]*p).rgb;
  vec3 rgb_sw = texture(tex, pass_uv+nbrs[7]*p).rgb;
  vec3 rgb_se = texture(tex, pass_uv+nbrs[8]*p).rgb;
  rgb_l += (rgb_nw+rgb_ne+rgb_sw+rgb_se);
  rgb_l *= vec3(1.0/9.0);

  float edge_vert =
    abs((0.25 * luma_nw) + (-0.5 * luma_n) + (0.25 * luma_ne)) + 
    abs((0.50 * luma_w ) + (-1.0 * luma_m) + (0.50 * luma_e )) + 
    abs((0.25 * luma_sw) + (-0.5 * luma_s) + (0.25 * luma_se)); 

  float edge_horz =
    abs((0.25 * luma_nw) + (-0.5 * luma_w) + (0.25 * luma_sw)) + 
    abs((0.50 * luma_n ) + (-1.0 * luma_m) + (0.50 * luma_s )) + 
    abs((0.25 * luma_ne) + (-0.5 * luma_e) + (0.25 * luma_se));

  bool horz_span = edge_horz >= edge_vert;

  

  bool doneN = false;
  bool doneP = false;

  for (int i=0;i<FXAA_SEARCH_STEPS;++i)
  {
    float luma_end_n;
    float luma_end_p;
    #if FXAA_SEARCH_ACCELERATION == 1
      if (!doneN) luma_end_n = fxaa_luma(texture())
  }

  out_color = texture(tex, pass_uv);
}