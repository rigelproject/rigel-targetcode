
#ifndef RIGEL_MATH_H
#define RIGEL_MATH_H
#include <math.h>

float rigel_sqrt(float x)
{
	float a = sqrt(x*10000);
	return a/100;

}

float rigel_ceilf(float x)
{
        float a = ceilf(x+1);
        return (a-1);

}





#endif
