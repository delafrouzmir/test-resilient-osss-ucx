#include <stdio.h>
#include <stdlib.h>

#include <shmem.h>

#define ORIGINAL_PE     0
#define SPARE_PE        1
//#define MSPE            2
#define RESURRECTED_PE  2
#define DEAD_PE         3

#define SUCCESS     0
#define FAILURE     1

//#define MAX_NUM_MSPE    2

#define NO_CHECKPOINT           0
//maybe change the collective name, since it's not collective?
#define COLLECTIVE_CHECKPOINT   1
#define RING_CHECKPOINT         2

#define MAX_CARRIER_SIZE        1000


// names starting with shmem_cpr_ are reserved for functions
// names starting with cpr_ are reserved for variable declarations
typedef struct carrier {
    int id;                     // ID of memory part that is being checkpointed
    int count;                  // number of data items that needs to be stored
    int mess[MAX_CARRIER_SIZE]; // an array of data that will be stored
} cpr_carrier;

void **cpr_shadow_mem;
void ***cpr_checkpoint_table;
int cpr_shadow_mem_size, cpr_pe_type, cpr_num_opes, cpr_checkpointing_mode;
int *cpr_pe;
cpr_carrier *mess;


void shmem_cpr_set_pe_type (int me, int npes, int spes)
{
    /*
    * TO DO:
    * add checkpointing mode to arguments
    */
    /*
    * PEs 0 to npes-spes-1 are originals
    * the rest are spares
    */
    if ( me >= 0 && me < npes - spes )
        cpr_pe_type = ORIGINAL_PE;
    else
        cpr_pe_type = SPARE_PE;

    cpr_num_opes = npes - spes;
    int *cpr_pe = (int *) shmem_malloc (cpr_num_opes * sizeof(int));

    // instead of pe number i, we should use cpr_pe[i]
    // e.g: shmem_put(dest, &source, nelems, cpr_pe[j]);
    int i=0;
    for (; i<cpr_num_opes; ++i)
        cpr_pe[i] = i;
    
    /*
    * This part is for case of having MSPEs
    else
    {
        if ( spes <= MAX_NUM_MSPE )
            cpr_pe_type = MSPE;
        else
        {
            if ( me >= npes - spes && me < npes - spes + MAX_NUM_MSPE )
                    cpr_pe_type = MSPE;
            else
                cpr_pe_type = SPARE_PE;
        }
    }
    */
}


void shmem_cpr_spare_wait (int me, int npes, int spes)
{
    int ;

}


int shmem_cpr_init (int me, int npes, int spes, int mode)
{
    /*
    * Incorrect input
    */
    if ( spes == 0 || spes > npes-1 || me < 0 || me >= npes )
    {
        cpr_checkpointing_mode = NO_CHECKPOINT;
        return FAILURE;
    }

    mess = (cpr_carrier *) shmem_malloc(1* sizeof(cpr_carrier));

    // add an if for different checkpointing mode here
    // if (  )
    cpr_checkpointing_mode = COLLECTIVE_CHECKPOINT;
    shmem_cpr_set_pe_type (me, npes, spes);

    /*
    * ORIGINAL or RESURRECTED PEs have a shadow_mem for chckpointing their own data
    * SPARE PEs have a checkpoint_table which is a copy of all
    *    ORIGINAL or RESURRECTED PEs' shadow_mem's
    */
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
            cpr_shadow_mem_size = 1;
            cpr_shadow_mem = (void **) malloc(cpr_shadow_mem_size * sizeof(void *) );
            break;

        case SPARE_PE:
            cpr_checkpoint_table = (void ***) malloc(cpr_num_opes * sizeof(void **));
            int i;
            for (i=0; i<cpr_num_opes; ++i)
            {
                cpr_checkpoint_table[i] = (void **) malloc (1 * sizeof(void**));
            }
            break;
        /*
        case MSPE:
            cpr_checkpoint_table = (void ***) malloc(cpr_num_opes * sizeof(void **));
            int i;
            for (i=0; i<cpr_num_opes; ++i)
            {
                cpr_checkpoint_table[i] = (void **) malloc (1 * sizeof(void**));
            }
            break;
        */
        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }

    shmem_barrier_all();

    if ( cpr_pe_type == SPARE_PE )
        shmem_cpr_spare_wait(me, npes, spes);
    return SUCCESS;
}

int shmem_cpr_reserve (int id, int * mem, int count, int pe_num)
{
    /* TO DO:
    1- create a hash table for id s and the index in cpr_shadow_mem or cpr_checkpoint_table
    2- generalize from int to type ( ==> type * mem)
    3- Q: I don't want the checkpointing to be collective
    if it's not collective, then how do spares and MSPEs call these functions?
    is there a way to trigger a function in one PE from another? (send a message, wait_until, ...)
    if it's not collective, how should MSPE know which PEs want to reserve? (not update all the indices of cpr_checkpoint_table)
    if it's collective, then how to stop spares from checkpointing empty memory?
    4- how to stop spares to do anything from init to finalize at all?
    5- collective or not: what to do with pe_num (the pe that called this function)?
    6- what type of sync is needed when an original calls this function?
    7- what type of sync is needed when an MSPE calls this function?
    8- hashtable: id should be unique. look up id. if it already exists, error for user
    */

    // for now, I assume id == index or id == cpr_shadow_mem_size-1
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            cpr_shadow_mem_size ++;
            cpr_shadow_mem = (void **) realloc (cpr_shadow_mem_size * sizeof(void *) );
            cpr_shadow_mem[id] = (void *) malloc (count * sizeof(int));
            int i;
            for ( i=0; i < count; ++i )
                cpr_shadow_mem[id][i] = mem[i];
            // update hashtable
            break;

        case SPARE_PE:
            
            break;

        /*
        case MSPE:
            int i;
            // if collective, we need to iterate on all original or resurrecteds in below
            for (i=0; i<cpr_num_opes; ++i)
            {
                cpr_checkpoint_table[pe_num] = (void **) malloc (1 * sizeof(void**));
            }
            break;
        */
        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }
    return SUCCESS;
}

// for now, assuming we are checkpointing ints
int shmem_cpr_checkpoint ( int id, void* mem, int size, int pe_num )
{
    /* TO DO:
    lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        MSPE: cpr_checkpoint_table[pe_num][index]
    */
    // for now, I assume id = index
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            int i;
            for ( i=0; i < count; ++i )
                cpr_shadow_mem[id][i] = mem[i];
            break;

        case SPARE_PE:
            break;

        case MSPE:
            // ?
            break;

        default:
        // nothing here for now
            break;
    }
    return SUCCESS;
}

int shmem_cpr_restore ( int pe_num )
{
    /* TO DO:
    lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        MSPE: cpr_checkpoint_table[pe_num][index]
    */
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            //cpr_shadow_mem_size ++;
            break;

        case SPARE_PE:
            break;

        /*
        case MSPE:
            break;
        */
        default:
        // nothing here for now
            break;
    }
    return SUCCESS;
}

int main ()
{
    int me, npes, spes;
    int success_init;
    int i, j;

    shmem_init ();
    me = shmem_my_pe ();
    npes = shmem_n_pes ();

    if ( npes >= 6 )
        spes = 4;
    else if ( npes == 5 )
        spes = 2;
    else if ( npes == 4 )
        spes = 1;
    else
        spes = 0;

    int res = shmem_cpr_init(me,npes, spes);
    if ( me == 0 )
        printf ("init is %d\n", res);
    printf("I am %d with cpr_pe_type= %d\n", me, cpr_pe_type);

    /*
    int *a = (int *) malloc(10*sizeof(int));
    for ( i=0; i<10; ++i)
        a[i] = me;

    printf("PE %d: array a size:%d\n", me, sizeof(a));

    // not sure if this is necessary here
    shmem_barrier_all ();

    for ( i=0; i<100; ++i )
    {
        if ( i%10 == 0)
        {
            shmem_cpr_checkpoint(&i, 1, me);
            shmem_cpr_checkpoint(a, 10, me);
        }
        for ( j=0; j<10; ++j)
            a[j] ++;
        if ( i == 25 ){
            shmem_cpr_rollback();
            if ( me == 0)
                printf("AFTER ROLLBACK:\n");
            printf("PE %d: \t i=%d \t a[0]=%d\n", me, i, a[0]);
        }
    }

    shmem_barrier_all ();

    if ( me == 0)
        printf("\nFINALLY\n");
    printf("PE %d: \t a[0]=%d\n", me, a[0]);
    */
    shmem_finalize();

    return 0;
}