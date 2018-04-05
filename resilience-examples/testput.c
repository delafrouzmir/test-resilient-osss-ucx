#include <stdio.h>
#include <assert.h>

#include <shmem.h>

int * mem;

int
main (int argc, char **argv)
{
    int me, npes;
    int src;

    shmem_init ();
    me = shmem_my_pe ();

    mem = (int *) malloc (10*sizeof(int));

    src = 5;

    shmem_barrier_all ();

    if ( me == 0 )
        shmem_int_put (&mem[5], &src, 1, 1);

    shmem_barrier_all ();

    if (1 == me) {
        int i;
        for (i=0; i<10; ++i)
            printf("%d\t", mem[i]);
        printf("\n");
    }
    
    shmem_barrier_all ();
    shmem_finalize ();

    return 0;
}
