#include <stdio.h>
#include <stdlib.h>

#include <shmem.h>

int *spr_pes;

int main(int argc, char const *argv[])
{
	int me, npes, i;

	shmem_init();
	me = shmem_my_pe();
	npes = shmem_n_pes();

	spr_pes = (int *) shmem_malloc ( npes * sizeof(int));

	for ( i=0; i<npes; ++i )
		spr_pes[i] = me;

	shmem_finalize();
	return 0;
}