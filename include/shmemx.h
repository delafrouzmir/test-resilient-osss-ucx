/* For license: see LICENSE file at top-level */

#ifndef _SHMEMX_H
#define _SHMEMX_H 1

#include <shmem.h>

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

    /*
     * new ideas (not part of formal 1.x API)
     */

    /*
     * wallclock time
     *
     */

    /**
     * @brief returns the number of seconds since the program
     * started running
     *
     * @section Synopsis:
     *
     * @substitute c C/C++
     * @code
     double shmemx_wtime(void)
     * @endcode
     *
     * @return Returns the number of seconds since the program started(epoch).
     *
     * @section Note: shmemx_wtime does not indicate any error code; if it
     * is unable to detect the elapsed time, the return value is
     * undefined.  The time may be different on each PE, but the epoch
     * from which the time is measured will not change while OpenSHMEM is
     * active.
     *
     */
    double shmemx_wtime(void);

    /*
     * profiling
     */
    void shmemx_pcontrol(int level);

    /*
     * address translation
     *
     */

    /**
     * @brief returns the symmetric address on another PE corresponding to
     * the symmetric address on this PE
     *
     * @section Synopsis:
     *
     * @substitute c C/C++
     * @code
     void *shmemx_lookup_remote_addr(void *addr, int pe)
     * @endcode
     *
     * @return Returns the address corresponding to "addr" on PE "pe"
     *
     *
     */
    void *shmemx_lookup_remote_addr(void *addr, int pe);

    /*
     * non-blocking fence/quiet
     *
     */

    /**
     * @brief check whether all communication operations issued prior to
     * this call have satisfied the fence semantic.
     *
     * @section Synopsis:
     *
     * @substitute c C/C++
     * @code
     int shmemx_fence_test(void)
     * @endcode
     *
     * @return Returns non-zero if all communication operations issued
     * prior to this call have satisfied the fence semantic, 0 otherwise.
     *
     */
    int shmemx_fence_test(void);

    /**
     * @brief check whether all communication operations issued prior to
     * this call have satisfied the quiet semantic.
     *
     * @section Synopsis:
     *
     * @substitute c C/C++
     * @code
     int shmemx_quiet_test(void)
     * @endcode
     *
     * @return Returns non-zero if all communication operations issued
     * prior to this call have satisfied the quiet semantic, 0 otherwise.
     *
     */
    int shmemx_quiet_test(void);

    /*
     * context sessions
     */
    void shmemx_ctx_start_session(shmem_ctx_t ctx);
    void shmemx_ctx_end_session(shmem_ctx_t ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _SHMEMX_H */
