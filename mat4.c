#include "mat4.h"
#include "vec3.h"

#define PI 3.1415926

#include <math.h>
#include <stdio.h>

void mat4_mul(float *m1, float *m2, float *m3)
{
    int i,j,k;
    for (i=0;i<4;++i) // COLUMN
    {
        for (j=0;j<4;++j) // ROW
        {
            m3[i*4+j] = 0;
            for (k=0;k<4;++k)
            {
                m3[i*4+j] += m1[k*4+j] * m2[i*4+k];
            }
        }
    }
}

void mat4_scale(float *m1, float *v1, float *m2)
{
    float m3[16];
    mat4_iden(m3);
    m3[0] = v1[0];
    m3[5] = v1[1];
    m3[10] = v1[2];
    mat4_mul(m1,m3,m2);
}

void mat4_iden(float *m1)
{
    int i;
    for (i=0;i<16;++i) m1[i] = i%5?0.0:1.0;
}

void mat4_pers(float *m1, float fovy, float aspect, float near, float far)
{
    mat4_iden(m1);
    float f = 1.0/tan(fovy*PI/180);
    m1[0] = f/aspect;
    m1[5] = f;
    float dif = near-far;
    m1[10] = (-near-far)/dif;
    m1[11] = 1;
    m1[14] = (2*far*near)/dif;
    m1[15] = 0;
}

void mat4_lookAt(float *m1, float *eye, float *center, float *up)
{
    float dir[3]; int i;
    for (i=0;i<3;++i) dir[i] = -center[i] + eye[i];
    float dirn[3];
    
    vec3_norm(dir, dirn);
    float upn[3];
    vec3_norm(up,upn);

    float right[3];
    vec3_cross(upn, dirn, right);

    float uptrue[3];
    vec3_cross(dirn, right, uptrue);

    vec3_norm(right, right);
    vec3_norm(uptrue, uptrue);

    mat4_iden(m1);
    m1[0] = right[0];m1[4] = right[1];m1[8] = right[2];
    m1[1] = uptrue[0];m1[5] = uptrue[1];m1[9] = uptrue[2];
    m1[2] = -dirn[0];m1[6] = -dirn[1];m1[10] = -dirn[2];
    m1[12] = -vec3_dot(right, eye);
    m1[13] = -vec3_dot(uptrue, eye);
    m1[14] = vec3_dot(dirn, eye);
}

