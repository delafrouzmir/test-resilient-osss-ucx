#include <stdio.h>
#include <stdlib.h>

typedef struct as
{
	int data_size;
	int *data;
} as;

void test_print ( as *as_test )
{
	printf("Size of test struct is %d\n", as_test->data_size);
	int i;
	for ( i = 0; i< as_test -> data_size; ++i)
		printf("%d ", as_test -> data[i]);
	printf("\n");
}

int main(int argc, char const *argv[])
{
	
	int *a = malloc ( 100 * sizeof(int));
	int i;
	for ( i=0; i<100; ++i)
		a[i] = i;

	as *as_test = malloc ( 1 * sizeof(as) );
	as_test -> data_size = 100;
	as_test -> data = malloc ( as_test -> data_size * sizeof (int) );
	for ( i=0; i< as_test -> data_size; ++i )
		as_test -> data[i] = a[i];

	test_print(as_test);

	return 0;
}