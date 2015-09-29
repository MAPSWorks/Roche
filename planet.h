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
	int has_ring;

	vec3 atmos_color;
	float cloud_epoch;

	Texture *day,*night,*clouds,*ring;
	Object *planet;
	Object *rings;

}
Planet;

void planet_render(Planet *planet, mat4 proj_mat, mat4 view_mat, vec3 view_dir, vec3 light_dir);

#endif