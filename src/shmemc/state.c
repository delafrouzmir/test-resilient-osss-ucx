/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "state.h"

/*
 * the PE's initial state
 */
thispe_info_t proc = {
    .status = SHMEMC_PE_UNKNOWN,
    .rank = -1,
    .refcount = 0,
};
