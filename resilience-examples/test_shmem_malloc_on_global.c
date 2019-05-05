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

	for ( i=0; i<npes; ++i )
		shmem_int_put ( me, &spr_pes[i], 1, (me+1)%npes);

	if ( me == 0 )
		for ( i=0; i<npes; ++i )
			printf("%d ",spr_pes[i]);

	printf("\n");
	shmem_finalize();
	return 0;
}