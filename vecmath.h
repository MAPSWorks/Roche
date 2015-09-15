#ifndef VECMATH_H
#define VECMATH_H

typedef struct 
{
    float v[3];
} vec3;

typedef struct 
{
    float v[4];
} quat;

typedef struct 
{
    float v[16];
} mat4;

mat4 mat4_mul(mat4 m1, mat4 m2);
mat4 mat4_scale(mat4 m, vec3 v);
mat4 mat4_iden();
mat4 mat4_pers(float fovy, float aspect, float near, float far);
mat4 mat4_lookAt(vec3 eye, vec3 dir, vec3 up);

vec3 vec3n(float x, float y, float z);
vec3 vec3_cross(vec3 v1, vec3 v2);
float vec3_dot(vec3 v1, vec3 v2);
float vec3_len(vec3 v);
vec3 vec3_norm(vec3 v);

quat quat_iden();
quat quat_mul(quat q1, quat q2);
quat quat_rot(vec3 axis, float angle);
mat4 quat_tomatrix(quat q);

#endif