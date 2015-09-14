#ifndef MAT4_H
#define MAT4_H

void mat4_mul(float *m1, float *m2, float *m3);
void mat4_scale(float *m1, float *v1, float *m2);
void mat4_iden(float *m1);
void mat4_pers(float *m1, float fovy, float aspect, float near, float far);
void mat4_lookAt(float *m1, float *eye, float *dir, float *up);

#endif