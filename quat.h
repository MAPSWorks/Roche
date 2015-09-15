#ifndef QUAT_H
#define QUAT_H

void quat_iden(float *q);
void quat_mul(float *q1,float *q2,float *q3);
void quat_rot(float *q, float *axis, float angle);
void quat_tomatrix(float *q, float *mat);


#endif