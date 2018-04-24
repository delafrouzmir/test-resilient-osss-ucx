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
#define MAX_CARRIER_QSIZE       1000

/*
#define SIG_DISABLE     0
#define SIG_INACTIVE    1
#define SIG_RESERVER    2
#define SIG_CHECKPOINT  3
#define SIG_RESTORE     4
*/

// names starting with shmem_cpr_ are reserved for functions
// names starting with cpr_ are reserved for variable declarations
typedef struct carrier {
    int id;                     // ID of memory part that is being reserved or checkpointed
    int count;                  // number of data items that needs to be stored
    int mess[MAX_CARRIER_SIZE]; // an array of data that will be stored, empty in case of reserve
    int pe_num;                 // the PE that asked for a reservation or checkpoint
} cpr_carrier;

void **cpr_shadow_mem;
void ***cpr_checkpoint_table;
cpr_carrier *mess, *cpr_res_qcarr, *cpr_check_qcarr;
int cpr_shadow_mem_size, cpr_pe_type, cpr_num_opes;
int cpr_num_spes, cpr_checkpointing_mode, cpr_sig, cpr_start;
int cpr_res_qcarr_head, cpr_res_qcarr_tail, cpr_check_qcarr_head, cpr_check_qcarr_tail;
int *cpr_pe, *cpr_table_size;


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

    // instead of pe number i, we should use cpr_pe[i]
    // e.g: shmem_put(dest, &source, nelems, cpr_pe[j]);
    int i=0;
    for (; i<cpr_num_opes; ++i)
        cpr_pe[i] = i;
    
}

/*
int shmem_cpr_spare_wait (int me, int npes, int spes)
{
    if ( cpr_pe_type != SPARE_PE )
        return FAILURE;

    int i, j, pe, id, count;
    while ( cpr_start == 1 )
    {
        // get the signal of every PE and check it needs reservation or checkpointing
        for ( i=0; i<cpr_num_opes; ++i )
        {
            pe = cpr_pe[i];
            shmem_get(&cpr_sig, &cpr_sig, 1, pe);
            switch (cpr_sig)
            {
                case SIG_RESERVER:
                    shmem_get(&mess, &mess, 1, pe);
                    cpr_table_size[pe]++;
                    cpr_checkpoint_table[pe] =
                                        (void **) realloc (cpr_table_size[pe] * sizeof(void**));
                    cpr_checkpoint_table[pe][cpr_table_size[pe]-1] =
                                        (void *) malloc (mess->count * sizeof(int));
                    break;

                case SIG_CHECKPOINT:
                    shmem_get(&mess, &mess, 1, pe);
                    id = mess->id;
                    count = mess->count;

                    for ( j=0; j<count; ++j)
                    {
                        cpr_checkpoint_table[pe][id][j] = mess->mess[j];
                    }
                    break;

                case SIG_RESTORE:
                    break;
                    
                case SIG_DISABLE:
                    return FAILURE;     // SIG_DISABLE is only for spare PEs and 
                                        // they should not appear in this process
                    break;

                case SIG_INACTIVE:
                default:
                break;
            }
        }
    }

}
*/

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

    cpr_res_qcarr = (cpr_carrier *) shmem_malloc (MAX_CARRIER_QSIZE * sizeof(cpr_carrier));
    cpr_check_qcarr = (cpr_carrier *) shmem_malloc (MAX_CARRIER_QSIZE * sizeof(cpr_carrier));
    cpr_res_qcarr_head = 0;
    cpr_res_qcarr_tail = 0;
    cpr_check_qcarr_head = 0;
    cpr_check_qcarr_tail = 0;
    cpr_start = 1;

    mess = (cpr_carrier *) shmem_malloc (1* sizeof(cpr_carrier));
    cpr_num_spes = spes;
    cpr_num_opes = npes - spes;
    cpr_pe = (int *) shmem_malloc (cpr_num_opes * sizeof(int));

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
            cpr_sig = SIG_INACTIVE;
            break;

        case SPARE_PE:
            cpr_checkpoint_table = (void ***) malloc(cpr_num_opes * sizeof(void **));
            cpr_table_size = (int *) malloc(cpr_num_opes * sizeof(int *));
            int i;
            for (i=0; i<cpr_num_opes; ++i)
            {
                cpr_table_size[i] = 1;
                cpr_checkpoint_table[i] = (void **) malloc (cpr_table_size[i] * sizeof(void**));
            }
            cpr_sig = SIG_DISABLE;
            break;

        case RESURRECTED_PE:
            return FAILURE;     // resurrected PEs won't call this function
        
        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }

    shmem_barrier_all();

    //  THIS IF IS FOR WHEN SPARES DO NOT PARTICIPATE IN RUNNING THE CODE
    //if ( cpr_pe_type == SPARE_PE )
    //    shmem_cpr_spare_wait(me, npes, spes);
    return SUCCESS;
}

int shmem_cpr_reserve (int id, int * mem, int count, int pe_num)
{
    /* TO DO:
    1- create a hash table for id s and the index in cpr_shadow_mem or cpr_checkpoint_table
    2- generalize from int to type ( ==> type * mem)
    6- what type of sync is needed when an original calls this function?
    8- hashtable: id should be unique. look up id. if it already exists, error for user
    */

    cpr_carrier *carr = (cpr_carrier *) malloc ( sizeof (cpr_carrier*) ); 
    int i, q_tail;

    // for now, I assume id == index or id == cpr_shadow_mem_size-1
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            cpr_shadow_mem_size ++;
            cpr_shadow_mem = (void **) realloc (cpr_shadow_mem_size * sizeof(void *) );
            cpr_shadow_mem[cpr_shadow_mem_size - 1] = (void *) malloc (count * sizeof(int));

            carr->id = id;
            carr->count = count;
            carr->pe_num = pe_num;

            printf("RESERVING from an ORIGINAL:\tid = %d,\tcount = %d, from pe = %d\n", id, count, pe_num);

            // should reserve a place on all spare PEs
            // and update the cpr_table_size of all spare PEs
            for ( i= npes-spes; i<npes; ++i)
            {
                // shmem_atomic_fetch_inc returns the amount before increment
                q_tail = ( shmem_atomic_fetch_inc ( cpr_res_qcarr_tail, i)) % MAX_CARRIER_QSIZE;
                shmem_put (cpr_res_qcarr[q_tail], carr, 1, i);

                printf("RESERVE carrier posted to pe %d with qtail=%d from pe %d\n", i, qtail, pe_num);
            }
            // update hashtable
            break;

        case SPARE_PE:

            printf("RESERVING from a SPARE:\treading %d carriers\n", cpr_res_qcarr_tail - cpr_res_qcarr_head);
            while (cpr_res_qcarr_head < cpr_res_qcarr_tail)
            {
                // head and tail might overflow the int size... add code to check
                carr = &cpr_res_qcarr[(cpr_res_qcarr_head % MAX_CARRIER_QSIZE)];
                cpr_res_qcarr_head ++;
                cpr_table_size[ carr-> pe_num] ++;
                cpr_checkpoint_table[carr-> pe_num] =
                            (void **) realloc (cpr_table_size[carr-> pe_num] * sizeof(void**));
                cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1] =
                            (void *) malloc (carr->count * sizeof(int));
                // update the hash table. I'm assuming id = index here
            }
            //return FAILURE;     // if SPAREs are not participated in code, they won't call reserve
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
        Spare: cpr_checkpoint_table[pe_num][index]
    */

    cpr_carrier *carr = (cpr_carrier *) malloc ( sizeof (cpr_carrier*) ); 
    int i, q_tail;

    // for now, I assume id = index
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            for ( i=0; i < count; ++i )
                cpr_shadow_mem[id][i] = mem[i];

            carr->id = id;
            carr->count = count;
            carr->pe_num = pe_num;
            for ( i=0; i < count; ++i )
                carr -> mem[i] = mem[i];

            printf("CHPING from an ORIGINAL:\tid = %d,\tcount = %d, from pe = %d\n", id, count, pe_num);
            
            // should reserve a place on all spare PEs
            // and update the cpr_table_size of all spare PEs
            for ( i= npes-spes; i<npes; ++i)
            {
                // shmem_atomic_fetch_inc returns the amount before increment
                q_tail = ( shmem_atomic_fetch_inc ( cpr_check_qcarr_tail, i)) % MAX_CARRIER_QSIZE;
                shmem_put (cpr_check_qcarr[q_tail], carr, 1, i);
                printf("CHP carrier posted to pe %d with qtail=%d from pe %d\n", i, qtail, pe_num);
            }
            // update hashtable
            break;

        case SPARE_PE:
            printf("CHPING from a SPARE:\treading %d carriers\n", cpr_check_qcarr_tail - cpr_check_qcarr_head);
            while (cpr_check_qcarr_head < cpr_check_qcarr_tail)
            {
                // head and tail might overflow the int size... add code to check
                carr = &cpr_check_qcarr[(cpr_check_qcarr_head % MAX_CARRIER_QSIZE)];
                cpr_check_qcarr_head ++;
                
                for ( i=0; i< carr-> count; ++i)
                    cpr_checkpoint_table[carr-> pe_num][carr-> id][i] = carr-> mess[i];
                // I'm assuming id = index here
            }
            //return FAILURE;     // if SPAREs are not participated in code, they won't call reserve
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
        spare: cpr_checkpoint_table[pe_num][index]
                update the cpr_pe and next_pe
    */
    int i, j;

    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            // if this PE is not the failed one
            if ( me != pe_num )
            {
                for ( i=0; i<cpr_shadow_mem_size; ++i)
                {

                }
            }
            break;

        case SPARE_PE:
            // if this PE is not the failed one
            if ( me != pe_num )
            {
                
            }
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
    int i, j, array_size;
    int *a;

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

    success_init = shmem_cpr_init(me,npes, spes);
    if ( me == 0 )
        printf ("init is %d\n", success_init);
    printf("I am %d with cpr_pe_type= %d\n", me, cpr_pe_type);

    array_size = 10 + me;
    a = (int *) malloc((array_size)*sizeof(int));
    for ( i=0; i<array_size; ++i)
        a[i] = me;

    printf("PE %d: array a size:%d\n", me, array_size);

    // not sure if this is necessary here
    shmem_barrier_all ();

    shmem_cpr_reserve(0, &i, 1, me);
    shmem_cpr_reserve(1, a, array_size, me);

    shmem_barrier_all();
    /*
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