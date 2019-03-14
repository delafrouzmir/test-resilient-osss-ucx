#include <stdio.h>
#include <assert.h>

#include <shmem.h>

int
main(void)
{
    int nextpe;
    int me, npes;
    int src;
    int *dest;
    int *dest_on_next_pe;

    shmem_init();
    me = shmem_my_pe();
    npes = shmem_n_pes();

    nextpe = (me + 1) % npes;

    src = nextpe;

    dest = (int *) shmem_malloc(sizeof(*dest));
    assert(dest != NULL);

    *dest = me;

    printf("The address of dest in PE=%d is %d and dest=%d\n", me, dest, *dest );

    shmem_barrier_all();

    dest_on_next_pe = (int *) shmem_ptr(dest, nextpe);

    shmem_barrier_all();

    printf("The address of dest in the next PE=%d is %d and it contains=%d\n", nextpe, dest_on_next_pe, *dest_on_next_pe);    

    shmem_barrier_all();
    shmem_free(dest);
    shmem_finalize();

    return 0;
}