/* For license: see LICENSE file at top-level */

#ifndef _RESILIENCE_H
#define _RESILIENCE_H 1

/* Defining different types of error for handling*/
#define SHMEM_SUCCESSFUL 0
#define SHMEM_NODE_FAILURE 1
#define SHMEM_PE_FAILURE 2
#define SHMEM_COMMUNICATION_FAILURE 3

/*
 * This status is returned from blocking operations
 * indicating if an error has happened during the op
*/
typedef struct shmemx_status {
    int source;				/* the number of PE that's source of error */
    int error_type;			/* type of error */
} shmemx_status_t;

/*
 * The default status is that the op was successfull
 * and no PE was source of failue ==> source=-1
*/
extern shmemx_status_t shmem_default_status;


/* Defining different types of PEs used for checkpointing */
#define ORIGINAL_PE 0
#define SPARE_PE 1
#define RESURRECTED_PE 2

#endif /* ! _RESILIENCE_H */
