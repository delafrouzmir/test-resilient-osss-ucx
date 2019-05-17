#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <shmem.h>

void mmul(const uint64_t Is, const uint64_t Ks, const uint64_t Js,
          const uint64_t Adist, const int64_t* A,
          const uint64_t Bdist, const int64_t* B,
          const uint64_t Cdist, int64_t* C) {

    uint64_t i, j, k;
    int64_t a_ik;

    for (i = 0; i < Is; i++) {
        for (k = 0; k < Ks; k++) {
            a_ik = A[i * Adist + k];
            for (j = 0; j < Js; j++) {
                C[i * Cdist + j] += (a_ik * B[k * Bdist + j]);
            }
        }
    }
}


void print_matrix(const int64_t* mat, const uint64_t Is, const uint64_t Js) {
    for (uint64_t i = 0; i < Is; i++) {
        for (uint64_t j = 0; j < Js; j++)
            printf("%d ", mat[i * Js + j]);
        printf("\n");
    }
}

int main(int argc, char const *argv[])
{
    int success_init;
    int i, j, k, l, array_size, first_rollback;
    //unsigned long* a;
    int *iter;
    clock_t start, end;

    int npes, spes, me, s, block_num, num_iter, frequency;
    unsigned long *As, *Bs, *Cs, *Bs_nxt, *temp;
    unsigned long* C;
    
    shmem_init ();
    me = shmem_my_pe ();
    npes = shmem_n_pes ();
    spes = 4;
    
    FILE *fp;
    fp = fopen ("new_matmul_res.txt", "a");

    iter = (int *) shmem_malloc(sizeof(int));
    shmem_barrier_all();

    start = clock();

    num_iter = atoi(argv[argc-1]);
    const unsigned long N = atoi(argv[argc-2]);           // Size of the matrices
    const unsigned long Ns = N / cpr_num_active_pes;   // Width of the stripes
    const unsigned long stripe_n_bytes = N * Ns * sizeof(unsigned long);

    // if ( cpr_pe_role == CPR_ACTIVE_ROLE ){
    As = (unsigned long*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of A
    Bs = (unsigned long*) shmem_align(4096, stripe_n_bytes);     // Vertical stripes of B
    Cs = (unsigned long*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of C
    
    Bs_nxt = (unsigned long*) shmem_align(4096, stripe_n_bytes); // Buffer that stores stripes of B
    temp = (unsigned long*) shmem_malloc (sizeof(int));

    // Initialize the matrices
    for(i = 0; i < N * Ns; i++) {
        As[i] = (i + me) % 5 + 1;
        Bs[i] = (i + me) % 5 + 3;
        Cs[i] = 0;
    }

    // array_size = atoi(argv[argc-1]);
    // a = (unsigned long *) shmem_malloc((array_size)*sizeof(unsigned long));
    // for ( i=0; i<array_size; ++i)
    //     a[i] = me;

    // not sure if this is necessary here
    shmem_barrier_all ();

    for ( (*iter)=0; (*iter)<num_iter; ++(*iter) )
    {
        if ( cpr_pe_role == CPR_ACTIVE_ROLE ){
            block_num = (me + s) % cpr_num_active_pes;

            shmem_getmem_nbi(Bs_nxt, Bs, stripe_n_bytes, (me + 1) % cpr_num_active_pes);

            mmul(Ns, N, Ns, N, As, Ns, Bs, N, Cs);

            temp = Bs;
            Bs = Bs_nxt;
            Bs_nxt = temp;
        }
    }

    shmem_barrier_all();
    if ( me == 0 )
        fprintf(fp, "no chp: npes=%d, array_size=%d, iter=%d\ntime=%f\n",
            npes, N, num_iter, (double) (clock()-start) / CLOCKS_PER_SEC);

    shmem_finalize();

    return 0;
}