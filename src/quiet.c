/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "shmemc.h"

#include "shmem_mutex.h"

#include "./include/shmem/resilience.h"

#ifdef ENABLE_PSHMEM
#pragma weak shmem_ctx_quiet = pshmem_ctx_quiet
#define shmem_ctx_quiet pshmem_ctx_quiet

#pragma weak shmem_ctx_fence = pshmem_ctx_fence
#define shmem_ctx_fence pshmem_ctx_fence
#endif /* ENABLE_PSHMEM */

shmemx_status_t
shmem_ctx_quiet(shmem_ctx_t ctx)
{
	shmemx_status_t op_status = shmem_default_status;

    SHMEMT_MUTEX_PROTECT(shmemc_ctx_quiet(ctx));

    return op_status;
}

shmemx_status_t
shmem_ctx_fence(shmem_ctx_t ctx)
{
	shmemx_status_t op_status = shmem_default_status;

    SHMEMT_MUTEX_PROTECT(shmemc_ctx_fence(ctx));

    return op_status;
}

#ifdef ENABLE_PSHMEM
#pragma weak shmem_quiet = pshmem_quiet
#define shmem_quiet pshmem_quiet

#pragma weak shmem_fence = pshmem_fence
#define shmem_fence pshmem_fence
#endif /* ENABLE_PSHMEM */

shmemx_status_t
shmem_quiet(void)
{
	shmemx_status_t op_status = shmem_default_status;

    SHMEMT_MUTEX_PROTECT(shmemc_ctx_quiet(SHMEM_CTX_DEFAULT));

    return op_status;
}

shmemx_status_t
shmem_fence(void)
{
	shmemx_status_t op_status = shmem_default_status;

	SHMEMT_MUTEX_PROTECT(shmemc_ctx_fence(SHMEM_CTX_DEFAULT));

	return op_status;
}
