#include <stdio.h>
#include <assert.h>

#include <shmem.h>

int
main(void)
{
    int me, npes, num;

    shmem_init();
    me = shmem_my_pe();
    npes = shmem_n_pes();

    int *counter = (int *) shmem_malloc(1 * sizeof(int));
    *counter = 5*me;
    printf("PE=%d, initial counter=%d\n", me, *counter);

    shmem_barrier_all();

    num = ( shmem_int_atomic_fetch_inc (counter, (me+1)%npes));
    printf("PE=%d, num=%d\n", me, num);

    shmem_barrier_all();

    printf("PE=%d, final counter=%d\n", me, *counter);    

    shmem_barrier_all();
    shmem_free(counter);
    shmem_finalize();

    return 0;
}