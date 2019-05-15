#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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


int main() {
    shmem_init();

    uint64_t npes, me, i, s, block_num;
    int64_t *As, *Bs, *Cs, *Bs_nxt, *temp;
    int64_t* C;
    clock_t start, end;
    
    npes = shmem_n_pes();
    me = shmem_my_pe();

    const uint64_t N = 4;           // Size of the matrices
    const uint64_t Ns = N / npes;   // Width of the stripes
    const uint64_t stripe_n_bytes = N * Ns * sizeof(int64_t);

    As = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of A
    Bs = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Vertical stripes of B
    Cs = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of C

    Bs_nxt = (int64_t*) shmem_align(4096, stripe_n_bytes); // Buffer that stores stripes of B
    temp = (int64_t*) shmem_malloc (sizeof(int));

    // Initialize the matrices
    for(i = 0; i < N * Ns; i++) {
        As[i] = (i + me) % 5 + 1;
        Bs[i] = (i + me) % 5 + 3;
        Cs[i] = 0;
    }

    // Make sure all the stripes are initialized
    shmem_barrier_all();

    start = clock();

    for (s = 0; s < npes; s++) {
        block_num = (me + s) % npes;

        shmem_getmem_nbi(Bs_nxt, Bs, stripe_n_bytes, (me + 1) % npes);

        mmul(Ns, N, Ns, N, As, Ns, Bs, N, Cs + block_num * Ns);

        temp = Bs;
        Bs = Bs_nxt;
        Bs_nxt = temp;

        shmem_barrier_all();
    }

    // Collect and print the matrix product
    if (me == 0) {
        C = (int64_t *) malloc (N * N * sizeof (int64_t));

        for (i = 0; i < Ns * N; i++)
            C[i] = Cs[i];

        for (i = 1; i < npes; i++)
            shmem_getmem_nbi(C + i * Ns * N, Cs, Ns * N * sizeof(int64_t), i);

        shmem_quiet();

        print_matrix(C, N, N);

        free (C);
    }

    shmem_barrier_all();
    end = clock();

    if ( me == 0 )
        printf("%f\n", (double)(end - start) / CLOCKS_PER_SEC);

    shmem_free(As);
    shmem_free(Bs);
    shmem_free(Cs);

    shmem_finalize();

    return 0;
}
