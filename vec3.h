#ifndef VEC3_H
#define VEC3_H

#include <math.h>

void vec3_cross(float *v1, float *v2, float *v3)
{
    v3[0] = v1[1]*v2[2] - v1[2]*v2[1];
    v3[1] = v1[2]*v2[0] - v1[0]*v2[2];
    v3[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

float vec3_dot(float *v1, float *v2)
{
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

float vec3_len(float *v1)
{
    return sqrt(vec3_dot(v1,v1));
}

void vec3_norm(float *v1, float *v2)
{
    float l = 1.0/vec3_len(v1);
    int i;
    for (i=0;i<3;++i) v2[i] = v1[i] * l;
}

#endif