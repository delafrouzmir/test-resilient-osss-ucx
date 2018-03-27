/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "shmemc.h"

#include "shmem/defs.h"

#ifdef ENABLE_PSHMEM
#pragma weak shmem_ptr = pshmem_ptr
#define shmem_ptr pshmem_ptr
#endif /* ENABLE_PSHMEM */

void *
shmem_ptr(const void *target, int pe)
{
    return shmemc_ctx_ptr(SHMEM_CTX_DEFAULT, target, pe);
}
