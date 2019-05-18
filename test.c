#include <stdio.h>

#include <shmem.h>
//#include "./include/shmem/resilience.h"

int main(int argc, char const *argv[])
{
	int me;
	shmem_init();

	me = shmem_my_pe();
	printf("hello from %d\n", me);

	//printf ("SHMEM_NODE_FAILURE = %d\n", SHMEM_NODE_FAILURE);

	//if ( me == 0 )
	//	printf("the default status is %d and %d\n", shmem_default_status.source, shmem_default_status.error_type);

	//if ( me == 1 ){
	//	shmemx_status_t op_status = {1, SHMEM_COMMUNICATION_FAILURE};
	//	printf("the op status is %d and %d\n", op_status.source, op_status.error_type);
	//}
	shmem_finalize();
	return 0;
}
