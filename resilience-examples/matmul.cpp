#include <iostream>
#include <utility>

#include <shmem.h>


void mmul(const uint64_t Is, const uint64_t Ks, const uint64_t Js,
          const uint64_t Adist, const int64_t* A,
          const uint64_t Bdist, const int64_t* B,
          const uint64_t Cdist, int64_t* C) {
    for (uint64_t i = 0; i < Is; i++) {
        for (uint64_t k = 0; k < Ks; k++) {
            int64_t a_ik = A[i * Adist + k];
            for (uint64_t j = 0; j < Js; j++) {
                C[i * Cdist + j] += a_ik * B[k * Bdist + j];
            }
        }
    }
}


void print_matrix(const int64_t* mat, const uint64_t Is, const uint64_t Js) {
    for (uint64_t i = 0; i < Is; i++) {
        for (uint64_t j = 0; j < Js; j++)
            std::cout << mat[i * Js + j] << ' ';
        std::cout << '\n';
    }
}


int main() {
    shmem_init();

    uint64_t npes = shmem_n_pes();
    uint64_t me = shmem_my_pe();

    const uint64_t N = 4;           // Size of the matrices
    const uint64_t Ns = N / npes;   // Width of the stripes
    const uint64_t stripe_n_bytes = N * Ns * sizeof(int64_t);

    int64_t* As = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of A
    int64_t* Bs = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Vertical stripes of B
    int64_t* Cs = (int64_t*) shmem_align(4096, stripe_n_bytes);     // Horizontal stripes of C

    int64_t* Bs_nxt = (int64_t*) shmem_align(4096, stripe_n_bytes); // Buffer that stores stripes of B

    // Initialize the matrices
    for(uint64_t i = 0; i < N * Ns; i++) {
        As[i] = (i + me) % 5 + 1;
        Bs[i] = (i + me) % 5 + 3;
        Cs[i] = 0;
    }

    // Make sure all the stripes are initialized
    shmem_barrier_all();

    for (uint64_t s = 0; s < npes; s++) {
        uint64_t block_num = (me + s) % npes;

        shmem_getmem_nbi(Bs_nxt, Bs, stripe_n_bytes, (me + 1) % npes);

        mmul(Ns, N, Ns, N, As, Ns, Bs, N, Cs + block_num * Ns);

        std::swap(Bs, Bs_nxt);

        shmem_barrier_all();
    }

    // Collect and print the matrix product
    if (me == 0) {
        int64_t* C = new int64_t[N * N];

        for (uint64_t i = 0; i < Ns * N; i++)
            C[i] = Cs[i];

        for (uint64_t i = 1; i < npes; i++)
            shmem_getmem_nbi(C + i * Ns * N, Cs, Ns * N * sizeof(int64_t), i);

        shmem_quiet();

        print_matrix(C, N, N);

        delete[] C;
    }

    shmem_barrier_all();

    shmem_free(As);
    shmem_free(Bs);
    shmem_free(Cs);

    shmem_finalize();

    return 0;
}
