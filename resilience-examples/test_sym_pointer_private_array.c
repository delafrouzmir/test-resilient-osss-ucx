#include <stdio.h>
#include <assert.h>

#include <shmem.h>

typedef struct a {
    int count;
    int *arr;
} ex;

int
main(void)
{
    int me, npes;
    int i;
    int *dest;
    int *dest_on_next_pe;
    ex *example;

    shmem_init();
    me = shmem_my_pe();
    npes = shmem_n_pes();

    example = (ex *) shmem_malloc(sizeof(ex *));
    example -> arr = (int *) malloc (10*sizeof(int));
    
    if ( me == 0 )
    {
        example -> count = 10;
        for ( i=0; i<10; ++i )
            example -> arr[i] = i*2;
        printf("address to example in pe 0 is %d\n", example);
        shmem_putmem (example, (void *) example, 1 * sizeof(ex), 1);
    }

    shmem_barrier_all();

    if ( me == 1 )
    {
        printf("address to example in pe 1 is %d\n", example);
        printf("in PE 1: example->count = %d\n", example-> count);
        for ( i=0; i<10; ++i )
            printf("example->arr[%d] = %d, ", i, example->arr[i] );
    }

    shmem_barrier_all();

    shmem_free(example);
    shmem_finalize();

    return 0;
}