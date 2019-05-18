#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main ()
{
	int a, b, c, d;

	a = 32;
	b = 16;
	c = 10;
	d = ceil( log( (double)(c+a)/b) / log(2) );
	printf("%d\n", d);

	return 0;
}