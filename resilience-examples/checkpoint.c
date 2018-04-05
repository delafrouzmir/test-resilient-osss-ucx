#include <stdio.h>
#include <stdlib.h>

#include <shmem.h>

#define ORIGINAL_PE     0
#define SPARE_PE        1
#define MSPE            2
#define RESURRECTED_PE  3
#define DEAD_PE         4

#define SUCCESS     0
#define FAILURE     1

#define MAX_NUM_MSPE    2

#define NO_CHECKPOINT           0
#define COLLECTIVE_CHECKPOINT   1
#define RING_CHECKPOINT         2


void **shadow_mem;
void ***checkpoint_table;
int shadow_mem_size, pe_type, num_opes, checkpointing_mode;


void shmem_cpr_set_pe_type (int me, int npes, int spes)
{

    /*
    * (npes-spes) PEs are the original ones, the rest:
        * If the nummber of spare PEs is less than or equal to 2,
        * then all of the spare PEs are Main Spare PEs (MSPE).
        * Else, 2 of spare PEs are MSPEs, the rest are just spares.
    */
    if ( me >= 0 && me < npes - spes )
        pe_type = ORIGINAL_PE;
    else
    {
        if ( spes <= MAX_NUM_MSPE )
            pe_type = MSPE;
        else
        {
            if ( me >= npes - spes && me < npes - spes + MAX_NUM_MSPE )
                    pe_type = MSPE;
            else
                pe_type = SPARE_PE;
        }
    }
}


int shmem_cpr_init (int me, int npes, int spes)
{
    /*
    * Incorrect input
    */
    if ( spes == 0 || spes > npes-1 || me < 0 || me >= npes )
    {
        checkpointing_mode = NO_CHECKPOINT;
        return FAILURE;
    }

    num_opes = npes - spes;
    shmem_cpr_set_pe_type (me, npes, spes);

    switch (pe_type)
    {
        case ORIGINAL_PE:
            shadow_mem_size = 1;
            shadow_mem = (void **) malloc(shadow_mem_size * sizeof(void *) );
            break;

        case SPARE_PE:
            // can we add something here that prevents spares
            // from running the code between shmem_cpr_init and finalize as others?
            break;

        case MSPE:
            checkpoint_table = (void ***) malloc(num_opes * sizeof(void **));
            int i;
            for (i=0; i<num_opes; ++i)
            {
                checkpoint_table[i] = (void **) malloc (1 * sizeof(void**));
            }
            break;

        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }

    shmem_barrier_all();
    return SUCCESS;
}

int shmem_cpr_reserve (int id, int * mem, int count, int pe_num)
{
    /* TO DO:
    1- create a hash table for id s and the index in shadow_mem or checkpoint_table
    2- generalize from int to type ( ==> type * mem)
    3- Q: I don't want the checkpointing to be collective
    if it's not collective, then how do spares and MSPEs call these functions?
    is there a way to trigger a function in one PE from another? (send a message, wait_until, ...)
    if it's not collective, how should MSPE know which PEs want to reserve? (not update all the indices of checkpoint_table)
    if it's collective, then how to stop spares from checkpointing empty memory?
    4- how to stop spares to do anything from init to finalize at all?
    5- collective or not: what to do with pe_num (the pe that called this function)?
    6- what type of sync is needed when an original calls this function?
    7- what type of sync is needed when an MSPE calls this function?
    8- hashtable: id should be unique. look up id. if it already exists, error for user
    */

    // for now, I assume id == index or id == shadow_mem_size-1
    switch (pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            shadow_mem_size ++;
            shadow_mem = (void **) realloc (shadow_mem_size * sizeof(void *) );
            shadow_mem[id] = (void *) malloc (count * sizeof(int));
            int i;
            for ( i=0; i < count; ++i )
                shadow_mem[id][i] = mem[i];
            // update hashtable
            break;

        case SPARE_PE:
            // nothing for now
            break;

        case MSPE:
            // TO DO:
            // collective or not :((((((((
            // this for loop should not be from 0 to opes
            // it should be for all original or resurrected ones
            // and their number may not be in order

            // if not collective: get the shadow_mem_size from pe pe_num (= pe_sh_size)
            // realloc checkpoint_table[pe_num] to one size bigger
            // copy mem to checkpoint_table[pe_num][id]
            int i;
            // if collective, we need to iterate on all original or resurrecteds in below
            for (i=0; i<num_opes; ++i)
            {
                checkpoint_table[pe_num] = (void **) malloc (1 * sizeof(void**));
            }
            break;

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
        MSPE: checkpoint_table[pe_num][index]
    */
    // for now, I assume id = index
    switch (pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            int i;
            for ( i=0; i < count; ++i )
                shadow_mem[id][i] = mem[i];
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
        MSPE: checkpoint_table[pe_num][index]
    */
    switch (pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            //shadow_mem_size ++;
            break;

        case SPARE_PE:
            break;

        case MSPE:
            break;

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
    printf("I am %d with pe_type= %d\n", me, pe_type);

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