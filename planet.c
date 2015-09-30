#include "planet.h"
#include "vecmath.h"
#include "opengl.h"

#include <stdlib.h>

mat4 computeLightMatrix(vec3 light_dir, vec3 light_up, float planet_size, float ring_outer)
{
    mat4 light_mat = mat4_iden();
    light_up = vec3_norm(light_up);
    light_dir = vec3_inv(vec3_norm(light_dir));
    vec3 light_right = vec3_norm(vec3_cross(light_dir, light_up));
    light_dir = vec3_mul(light_dir,ring_outer);
    light_up = vec3_mul(vec3_norm(vec3_cross(light_dir, light_right)), planet_size);
    light_right = vec3_mul(light_right, planet_size);
    int i;
    for (i=0;i<3;++i)
    {
        light_mat.v[i*4] = light_right.v[i];
        light_mat.v[i*4+1] = light_up.v[i];
        light_mat.v[i*4+2] = -light_dir.v[i];
    }
    return light_mat;
}

void computeRingMatrix(vec3 toward_view, vec3 rings_up, float size, mat4 *near_mat, mat4 *far_mat)
{
    *near_mat = mat4_iden();
    *far_mat = mat4_iden();
    rings_up = vec3_norm(rings_up);
    toward_view = vec3_norm(toward_view);

    vec3 rings_right = vec3_norm(vec3_cross(rings_up, toward_view));
    vec3 rings_x = vec3_norm(vec3_cross(rings_up, rings_right));
    int i;
    for (i=0;i<3;++i)
    {
        near_mat->v[i] = rings_x.v[i]*size;
        near_mat->v[4+i] = rings_right.v[i]*size;
        near_mat->v[8+i] = rings_up.v[i]*size;
        far_mat->v[i] = -rings_x.v[i]*size;
        far_mat->v[4+i] = -rings_right.v[i]*size;
        far_mat->v[8+i] = -rings_up.v[i]*size;
    }
}

#define RING_ITERATIONS 100

void generate_rings(unsigned char *buffer, int size, int seed)
{
    // Starting fill
    int i,j;
    const int ref_size = 4096;
    float *ref_buffer = malloc(sizeof(float)*ref_size);
    for (i=0;i<ref_size;++i)
    {
        ref_buffer[i] = 1.0;
    }
    srand(seed);

    // gap generation
    const int max_gapsize = ref_size/20;
    for (i=0;i<RING_ITERATIONS;++i)
    {
        int gapsize = rand()%(max_gapsize);
        int gap = rand()%(ref_size-gapsize+1);
        float gap_opacity = rand()%RAND_MAX/(float)RAND_MAX;
        if (gap_opacity < 0.4) gap_opacity = 0.4;
        for (j=gap;j<gap+gapsize;++j)
        {
            ref_buffer[j] *= gap_opacity;
        }
    }
    // brightness equalization
    float mean = 0;
    for (i=0;i<ref_size;++i)
    {
        mean += ref_buffer[i];
    }
    mean /= ref_size;
    float mul = 1.0/mean;
    for (i=0;i<ref_size;++i)
    {
        ref_buffer[i] *= mul;
    }

    // fading
    const int fade = ref_size/10;
    for (i=0;i<fade;++i)
    {
        ref_buffer[ref_size-i-1] *= i/(float)fade; 
        ref_buffer[i] *= i/(float)fade;
    }
    float scale = ref_size/(float)size;
    for (i=0;i<size;++i)
    {
        float mean = 0.0;
        for (j=i*scale;j<(i+1)*scale;++j)
        {
            mean += ref_buffer[j];
        }
        mean /= scale;
        buffer[i] = (unsigned char)(mean*255);
    }
    free(ref_buffer);
}

#define PLANET_STRIDE 24

void render_planet()
{
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,PLANET_STRIDE,(GLvoid*)16);
}

void render_rings()
{
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,24,(GLvoid*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,24,(GLvoid*)16);
}

void planet_load(Planet *p)
{
    const int ringsize = 2048;
    unsigned char *rings = malloc(ringsize);
    generate_rings(rings, ringsize, p->ring_seed);

    create_tex(&p->ring);
    image_tex(&p->ring, 1, ringsize, 1, (void*)rings);
    free(rings);
    
    tex_load_from_file(&p->day, p->day_filename, 3);
    tex_load_from_file(&p->clouds, p->clouds_filename, 3);
    tex_load_from_file(&p->night, p->night_filename, 3);
}

void planet_render(Planet *p, mat4 proj_mat, mat4 view_mat, vec3 view_pos, vec3 light_dir, Shader *planet_shader, Shader *ring_shader, Object *planet_obj, Object *ring_obj)
{
 	quat q = quat_rot(p->rot_axis, p->rot_epoch);
    mat4 planet_mat = quat_tomatrix(q);
    planet_mat = mat4_scale(planet_mat, vec3_mul(vec3n(1,1,1),p->radius));

    mat4 light_mat = computeLightMatrix(light_dir, vec3n(0,0,1), p->radius, p->ring_outer);
    
    mat4 far_ring_mat, near_ring_mat;
    computeRingMatrix(vec3_add(p->pos, vec3_inv(view_pos)), p->ring_upvector, p->ring_outer, &near_ring_mat, &far_ring_mat);

    if (p->has_rings)
    {
	    // FAR RING RENDER
	    use_shader(ring_shader);
	    uniform(ring_shader, "projMat", proj_mat.v);
	    uniform(ring_shader, "viewMat", view_mat.v);
	    uniform(ring_shader, "modelMat", far_ring_mat.v);
	    uniform(ring_shader, "lightMat", light_mat.v);
	    uniform(ring_shader, "ring_color", p->ring_color.v);
	    uniform1i(ring_shader, "tex", 0);
	    uniform1f(ring_shader, "minDist", p->ring_inner/p->ring_outer);
	    use_tex(&p->ring,0);
	    render_obj(ring_obj, render_rings);
    }

    // PLANET RENDER
    use_shader(planet_shader);
    uniform(planet_shader, "projMat", proj_mat.v);
    uniform(planet_shader, "viewMat", view_mat.v);
    uniform(planet_shader, "modelMat", planet_mat.v);
    uniform(planet_shader, "ring_vec", p->ring_upvector.v);
    uniform(planet_shader, "light_dir", light_dir.v);
    uniform1f(planet_shader, "cloud_disp", p->cloud_epoch);
    uniform(planet_shader, "view_pos", view_pos.v);
    uniform(planet_shader, "sky_color", p->atmos_color.v);
    uniform1f(planet_shader, "ring_inner", p->ring_inner);
    uniform1f(planet_shader, "ring_outer", p->ring_outer);
    uniform1i(planet_shader, "day_tex", 0);
    uniform1i(planet_shader, "clouds_tex", 1);
    uniform1i(planet_shader, "night_tex", 2);
    uniform1i(planet_shader, "ring_tex", 3);
    use_tex(&p->day,0);
    use_tex(&p->clouds,1);
    use_tex(&p->night,2);
    use_tex(&p->ring, 3);
    render_obj(planet_obj, render_planet);

    if (p->has_rings)
    {
	    // NEAR RING RENDER
	    use_shader(ring_shader);
	    uniform(ring_shader, "projMat", proj_mat.v);
	    uniform(ring_shader, "viewMat", view_mat.v);
	    uniform(ring_shader, "modelMat", near_ring_mat.v);
	    uniform(ring_shader, "lightMat", light_mat.v);
	    uniform(ring_shader, "ring_color", p->ring_color.v);
	    uniform1i(ring_shader, "tex", 0);
	    uniform1f(ring_shader, "minDist", p->ring_inner/p->ring_outer);
	    use_tex(&p->ring,0);
	    render_obj(ring_obj, render_rings);
    }
}

void skybox_load(Skybox *s)
{
    tex_load_from_file(&s->tex, s->tex_filename, 3);
}

void skybox_render(Skybox *s, mat4 proj_mat, mat4 view_mat, Shader *skybox_shader, Object *o)
{
    quat skybox_rot = quat_rot(s->rot_axis, s->rot_angle/180.0*PI);
    mat4 skybox_mat = quat_tomatrix(skybox_rot);
    skybox_mat = mat4_scale(skybox_mat,vec3_mul(vec3n(1,1,1),s->size));
    
    // SKYBOX RENDER
    use_shader(skybox_shader);
    uniform(skybox_shader, "projMat", proj_mat.v);
    uniform(skybox_shader, "viewMat", view_mat.v);
    uniform(skybox_shader, "modelMat", skybox_mat.v);
    uniform1i(skybox_shader, "tex", 0);
    use_tex(&s->tex,0);
    render_obj(o, render_planet); 

}
