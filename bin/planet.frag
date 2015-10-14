#version 330
// PIXEL ATTRIBUTES
in vec2 pass_uv;
in vec3 pass_normal;
in vec4 pass_position;
in vec3 pass_lpos;
out vec4 out_color;

// TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D clouds_tex;
uniform sampler2D night_tex;
uniform sampler2D ring_tex;

// GLOBAL VARIABLES
uniform float cloud_disp; // Amount of displacement of cloud on x axis
uniform float ring_inner; // Ring's inner radius
uniform float ring_outer; // Ring's outer radius
uniform vec3 light_dir; // Towards the light source
uniform vec3 view_pos; // Position of the camera
uniform vec3 ring_vec; // Normal vector of the ring plane
uniform vec3 sky_color; // dat's right

// GAUSS KERNEL /W LERP
uniform float offset[3] = float[]( 0.0, 1.3846153846, 3.2307692308 );
uniform float weight[3] = float[]( 0.2270270270, 0.3162162162, 0.0702702703 );
// Ring texture size for gaussian offsets
float ringtex_size = 1.0/float(textureSize(ring_tex,0).x);

// This should be uniform variables
#define AMBIENT_LIGHT 0.04
#define RING_AMBIENT 0.2

void main(void)
{
	// TEXTURE LOOKUPS
	vec3 day = texture(day_tex,pass_uv).rgb;
	vec3 night = texture(night_tex, pass_uv).rgb;
	float cloud = texture(clouds_tex, pass_uv + vec2(cloud_disp,0.0)).r;

	// LIGHT CALCULATION
	float rawlight = dot(-light_dir, pass_normal);
	float light = clamp(rawlight,AMBIENT_LIGHT,1.0);
	
	// ATMOSPHERE RENDERING
	vec3 to_viewer = normalize(view_pos-pass_position.xyz);
	float angle = pow(1.0 - dot(pass_normal, to_viewer), 3);
	float rim = (angle*angle*angle);

	// TEXTURE COMPOSITION
	float nightlights = clamp(-rawlight*12.0+1.0,0.0,1.0);
	vec3 color = mix(day*light  + nightlights*night, vec3(light), cloud);

	// SHADOW CALCULATION (RAYTRACING)
	float t = dot(pass_lpos.xyz, ring_vec)/dot(light_dir,ring_vec);
	vec4 projPos = vec4(pass_lpos.xyz-t*light_dir,1.0);
	float dist = length(projPos.xyz);
	float shadow = 0.0;
	float tex_offset = (dist-ring_inner)/(ring_outer-ring_inner);
	for (int i=0;i<3;++i) // Size 5 gauss filter
	{
		shadow += texture(ring_tex, vec2(tex_offset + offset[i]*ringtex_size,0.0)).r * weight[i];
		shadow += texture(ring_tex, vec2(tex_offset - offset[i]*ringtex_size,0.0)).r * weight[i];
	}
	//shadow = 1.0 - (1.0-shadow)*light;
	shadow = mix(1.0,shadow*(1-RING_AMBIENT) + RING_AMBIENT,dist > ring_inner && dist < ring_outer && t>=0);
	color *= mix(shadow,1.0,nightlights);

	// ATMOSPHERE CALCULATION
	color = mix(color, sky_color*light, angle);
	out_color = vec4(mix(color, sky_color*2, rim*light), 1.0-(rim*rim*rim));
	
}