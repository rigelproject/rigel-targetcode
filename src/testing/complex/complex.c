/*conversion of a real number from its Cartesian to polar form*/

#include <stdio.h>
#include <complex.h>
#include "rigel.h"

int main(){
	if(RigelGetThreadNum() == 0) {
		SIM_SLEEP_OFF();
    double complex z = -4.4 + 3.3 * I;
    double radius = cabs(z);
    double argument = carg(z);

    double x = creal(z);
    double y = cimag(z);

    printf("cartesian(x,y):(%4.1f,%4.1f)\n",x,y);
    printf("polar(r,theta):(%4.1f,%4.1f)\n",radius,argument);
	}
  return 0;
}
