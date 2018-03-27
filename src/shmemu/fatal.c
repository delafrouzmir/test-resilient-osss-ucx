/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void
shmemu_fatal(const char *fmt, ...)
{
    const int complain = (proc.rank < 1);

    if (complain) {
        va_list ap;

        va_start(ap, fmt);
        fprintf(stderr, "*** FATAL: ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, " ***\n");
        fflush(stderr);
        va_end(ap);
    }

    exit(EXIT_FAILURE);
    /* NOT REACHED */
}
