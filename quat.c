#include "quat.h"
#include <math.h>

#include "mat4.h"

void quat_iden(float *q)
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}
void quat_mul(float *q1,float *q2, float *q3)
{
	int i, j;
	for (j=0;j<3;++j)
	{
		q3[j] = 0.0;
		for (i=0;i<3;++i)
		{
			int id = (i!=j)?((i+j>=4)?6-i-j:i+j):0 * (j>i)?1:-1;
			q3[j] += q2[j]*q1[id];
		}
	}
}
void quat_rot(float *q, float *axis, float angle)
{
	float s = sin(angle/2);
	int i;
	for (i=0;i<3;++i) q[i] = axis[i]*s;
	q[3] = cos(angle/2);
}

void quat_tomatrix(float *q, float *mat)
{
	mat4_iden(mat);
	mat[0] = 1-2*q[1]*q[1] - 2*q[2]*q[2];
	mat[5] = 1-2*q[0]*q[0] - 2*q[2]*q[2];
	mat[10] = 1-2*q[0]*q[0] - 2*q[1]*q[1];

	mat[1] = 2*q[0]*q[1] + 2*q[2]*q[3];
	mat[4] = 2*q[0]*q[1] - 2*q[2]*q[3];

	mat[6] = 2*q[1]*q[2] + 2*q[0]*q[3];
	mat[9] = 2*q[1]*q[2] - 2*q[0]*q[3];

	mat[2] = 2*q[0]*q[2] - 2*q[1]*q[3];
	mat[8] = 2*q[0]*q[2] + 2*q[1]*q[3];
}
