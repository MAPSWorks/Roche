#include "vecmath.h"

#define PI 3.1415926

#include <math.h>
#include <stdio.h>

mat4 mat4_mul(mat4 m1, mat4 m2)
{
    int i,j,k;
    mat4 m3;
    for (i=0;i<4;++i) // COLUMN
    {
        for (j=0;j<4;++j) // ROW
        {
            m3.v[i*4+j] = 0;
            for (k=0;k<4;++k)
            {
                m3.v[i*4+j] += m1.v[k*4+j] * m2.v[i*4+k];
            }
        }
    }
    return m3;
}

mat4 mat4_scale(mat4 m, vec3 v)
{
    mat4 m3 = mat4_iden();
    m3.v[0] = v.v[0];
    m3.v[5] = v.v[1];
    m3.v[10] = v.v[2];
    return mat4_mul(m, m3);
}

mat4 mat4_iden()
{
    int i;
    mat4 m1;
    for (i=0;i<16;++i) m1.v[i] = i%5?0.0:1.0;
    return m1;
}

mat4 mat4_pers(float fovy, float aspect, float near, float far)
{
    mat4 m1 = mat4_iden();
    float f = 1.0/tan(fovy*PI/180);
    m1.v[0] = f/aspect;
    m1.v[5] = f;
    float dif = near-far;
    m1.v[10] = (-near-far)/dif;
    m1.v[11] = 1;
    m1.v[14] = (2*far*near)/dif;
    m1.v[15] = 0;
    return m1;
}

mat4 mat4_lookAt(vec3 eye, vec3 center, vec3 up)
{
    vec3 dir = vec3_norm(vec3_add(eye, vec3_inv(center)));
    up = vec3_norm(up);

    vec3 right = vec3_cross(up, dir);
    up = vec3_cross(dir,right);

    right = vec3_norm(right);
    up = vec3_norm(up);

    mat4 m1 = mat4_iden();
    m1.v[0] = right.v[0];m1.v[4] = right.v[1];m1.v[8] = right.v[2];
    m1.v[1] = up.v[0];m1.v[5] = up.v[1];m1.v[9] = up.v[2];
    m1.v[2] = -dir.v[0];m1.v[6] = -dir.v[1];m1.v[10] = -dir.v[2];
    m1.v[12] = -vec3_dot(right, eye);
    m1.v[13] = -vec3_dot(up, eye);
    m1.v[14] = vec3_dot(dir, eye);
    return m1;
}

void mat4_print(mat4 m)
{
    printf("[%f,%f,%f,%f\n" , m.v[0] ,m.v[4] ,m.v[8] ,m.v[12]);
    printf(" %f,%f,%f,%f\n" , m.v[1] ,m.v[5] ,m.v[9] ,m.v[13]);
    printf(" %f,%f,%f,%f\n" , m.v[2] ,m.v[6] ,m.v[10],m.v[14]);
    printf(" %f,%f,%f,%f]\n", m.v[3] ,m.v[7] ,m.v[11],m.v[15]);
}

vec3 vec3n(float x, float y, float z)
{
    vec3 v;
    v.v[0] = x; 
    v.v[1] = y; 
    v.v[2] = z; 
    return v;
}

vec3 vec3_cpy(vec3 v)
{
    return vec3n(v.v[0], v.v[1], v.v[2]);
}

vec3 vec3_add(vec3 v1, vec3 v2)
{
    return vec3n(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1], v1.v[2] + v2.v[2]);
}

vec3 vec3_inv(vec3 v)
{
    return vec3n(-v.v[0], -v.v[1], -v.v[2]);
}

vec3 vec3_mul(vec3 v, float f)
{
    return vec3n(v.v[0]*f, v.v[1]*f, v.v[2]*f);
}

vec3 vec3_cross(vec3 v1, vec3 v2)
{
    vec3 v3;
    v3.v[0] = v1.v[1]*v2.v[2] - v1.v[2]*v2.v[1];
    v3.v[1] = v1.v[2]*v2.v[0] - v1.v[0]*v2.v[2];
    v3.v[2] = v1.v[0]*v2.v[1] - v1.v[1]*v2.v[0];
    return v3;
}

float vec3_dot(vec3 v1, vec3 v2)
{
    return v1.v[0]*v2.v[0] + v1.v[1]*v2.v[1] + v1.v[2]*v2.v[2];
}

float vec3_len(vec3 v1)
{
    return sqrt(vec3_dot(v1,v1));
}

vec3 vec3_norm(vec3 v)
{
    float l = 1.0/vec3_len(v);
    int i;
    vec3 v2;
    for (i=0;i<3;++i) v2.v[i] = v.v[i] * l;
    return v2;
}

quat quat_iden()
{
    quat q;
    q.v[0] = 0;
    q.v[1] = 0;
    q.v[2] = 0;
    q.v[3] = 1;
    return q;
}
quat quat_mul(quat q1, quat q2)
{
    int i, j;
    quat q3;
    for (j=0;j<3;++j)
    {
        q3.v[j] = 0.0;
        for (i=0;i<3;++i)
        {
            int id = (i!=j)?((i+j>=4)?6-i-j:i+j):0 * (j>i)?1:-1;
            q3.v[j] += q2.v[j]*q1.v[id];
        }
    }
    return q3;
}
quat quat_rot(vec3 axis, float angle)
{
    quat q;
    float s = sin(angle/2);
    int i;
    for (i=0;i<3;++i) q.v[i] = axis.v[i]*s;
    q.v[3] = cos(angle/2);
    return q;
}

mat4 quat_tomatrix(quat q)
{
    mat4 m = mat4_iden();
    m.v[0] = 1-2*q.v[1]*q.v[1] - 2*q.v[2]*q.v[2];
    m.v[5] = 1-2*q.v[0]*q.v[0] - 2*q.v[2]*q.v[2];
    m.v[10] =1-2*q.v[0]*q.v[0] - 2*q.v[1]*q.v[1];

    m.v[1] = 2*q.v[0]*q.v[1] + 2*q.v[2]*q.v[3];
    m.v[4] = 2*q.v[0]*q.v[1] - 2*q.v[2]*q.v[3];

    m.v[6] = 2*q.v[1]*q.v[2] + 2*q.v[0]*q.v[3];
    m.v[9] = 2*q.v[1]*q.v[2] - 2*q.v[0]*q.v[3];

    m.v[2] = 2*q.v[0]*q.v[2] - 2*q.v[1]*q.v[3];
    m.v[8] = 2*q.v[0]*q.v[2] + 2*q.v[1]*q.v[3];
    return m;
}
