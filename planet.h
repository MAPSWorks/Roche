#ifndef PLANET_H
#define PLANET_H

#include "vecmath.h"
#include "opengl.h"

typedef struct
{
	vec3 pos;
	vec3 rot_axis;
	float rot_epoch;
	float radius;

	float ring_inner;
	float ring_outer;
	vec3 ring_upvector;
	int ring_seed;
	vec3 ring_color;
	int has_rings;

	vec3 atmos_color;
	float cloud_epoch;

	Texture day,night,clouds,ring;
	char day_filename[1024];
	char night_filename[1024];
	char clouds_filename[1024];
}
Planet;

typedef struct
{
    Texture tex;
    char tex_filename[1024];
    vec3 rot_axis;
    float rot_angle;
    float size;
}
Skybox;

void planet_load(Planet *planet);
void planet_render(Planet *planet, mat4 proj_mat, mat4 view_mat, vec3 view_dir, vec3 light_dir, Shader *planet_shader, Shader *ring_shader, Object *planet_obj, Object *ring_obj);
void skybox_load(Skybox *s);
void skybox_render(Skybox *s, mat4 proj_mat, mat4 view_mat, Shader *skybox_shader, Object *o);

#endif
