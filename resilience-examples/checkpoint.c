#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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


// names starting with shmem_cpr_ are reserved for functions
// names starting with cpr_ are reserved for variable declarations
typedef struct resrv_carrier {
    int id;                     // ID of memory part that is being reserved
    int *adr;                   // Address of memory part that is being reserved
    int count;                  // number of data items that needs to be stored
    int pe_num;                 // the PE that asked for a reservation or checkpoint
} cpr_resrv_carrier;

typedef struct check_carrier {
    int id;                     // ID of memory part that is being checkpointed
    int *adr;                   // Address of memory part that is being checkpointed
    int count;                  // number of data items that needs to be stored
    int data[MAX_CARRIER_SIZE]; // an array of data that will be stored
    int pe_num;                 // the PE that asked for a reservation or checkpoint
} cpr_check_carrier;

typedef struct restr_carrier {
    int id;                     // ID of memory part that is being checkpointed
    int *adr;                   // Address of memory part that is being checkpointed
    int count;                  // number of data items that needs to be stored
    int data[MAX_CARRIER_SIZE]; // an array of data that will be stored
} cpr_restr_carrier;

// necessary checkpointing declarations
// Part 1: necessary for keeping checkpoints
cpr_check_carrier *cpr_shadow_mem;    // a PE's own copy of its checkpoints
cpr_check_carrier **cpr_checkpoint_table; // a spare PE's copy of all active PE's checkpoints
int cpr_shadow_mem_size, cpr_pe_type, cpr_num_active_pes, cpr_first_spare;
int cpr_num_spare_pes, cpr_checkpointing_mode, cpr_sig, cpr_start;
int *cpr_pe, *cpr_table_size;
// Part 2: necessary for exchange of data when reserving or checkpointing
cpr_check_carrier *cpr_check_queue;
int cpr_check_queue_head, cpr_check_queue_tail;
cpr_resrv_carrier *cpr_resrv_queue;
int cpr_resrv_queue_head, cpr_resrv_queue_tail;
// Part 3: necessary for restoration


// delarrations for the application and testing
int me, npes;
int called_check, called_resrv, posted_check, posted_resrv, read_check, read_resrv;

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
    // e.g: shmem_int_put(dest, &source, nelems, cpr_pe[j]);
    int i=0;
    for (; i<cpr_num_active_pes; ++i)
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
        for ( i=0; i<cpr_num_active_pes; ++i )
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

    // Initializing queues
    // Checkpointing queue:
    cpr_check_queue = (cpr_check_carrier *) shmem_malloc (MAX_CARRIER_QSIZE * sizeof(cpr_check_carrier));
    cpr_check_queue_head = 0;
    cpr_check_queue_tail = 0;

    // Reservation queue:
    cpr_resrv_queue = (cpr_resrv_carrier *) shmem_malloc (MAX_CARRIER_QSIZE * sizeof(cpr_resrv_carrier));
    cpr_resrv_queue_head = 0;
    cpr_resrv_queue_tail = 0;

    // Setting up numbers of active and spare PEs
    cpr_num_spare_pes = spes;
    cpr_num_active_pes = npes - spes;
    cpr_first_spare = cpr_num_active_pes;
    cpr_pe = (int *) shmem_malloc (cpr_num_active_pes * sizeof(int));

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
            cpr_shadow_mem = (cpr_check_carrier *) malloc(cpr_shadow_mem_size * sizeof(cpr_check_carrier) );
            //cpr_sig = SIG_INACTIVE;
            break;

        case SPARE_PE:
            cpr_checkpoint_table = (cpr_check_carrier **) malloc(cpr_num_active_pes * sizeof(cpr_check_carrier *));
            cpr_table_size = (int *) malloc(cpr_num_active_pes * sizeof(int *));
            int i;
            for (i=0; i<cpr_num_active_pes; ++i)
            {
                cpr_table_size[i] = 1;
                cpr_checkpoint_table[i] = (cpr_check_carrier *) malloc (cpr_table_size[i] * sizeof(cpr_check_carrier *));
            }
            break;

        case RESURRECTED_PE:
            return FAILURE;     // resurrected PEs won't call this function
        
        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }

    shmem_barrier_all();
    cpr_start = 1;
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

    cpr_resrv_carrier *carr = (cpr_resrv_carrier *) malloc ( sizeof (cpr_resrv_carrier*) ); 
    int i, q_tail;
    int npes = cpr_num_active_pes + cpr_num_spare_pes;

    // TEST purpose:
    called_resrv++;

    // for now, I assume id == index or id == cpr_shadow_mem_size-1
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            cpr_shadow_mem_size ++;
            cpr_shadow_mem = (cpr_check_carrier *) realloc (cpr_shadow_mem,
                                            cpr_shadow_mem_size * sizeof(cpr_check_carrier) );
            //USELESS: cpr_shadow_mem[cpr_shadow_mem_size - 1] = (void *) malloc (count * sizeof(int));

            // TODO: if count > 1000, I have to send (count/1000)+1 carriers
            carr->id = id;
            carr->count = count;
            carr->pe_num = pe_num;
            carr->adr = mem;

            // printf("RESERVING from an ORIGINAL:\tid = %d,\tcount = %d, from pe = %d\n", id, count, pe_num);

            // should reserve a place on all spare PEs
            // and update the cpr_table_size of all spare PEs
            for ( i= cpr_first_spare; i < npes; ++i)
            {
                // shmem_int_atomic_fetch_inc returns the amount before increment
                q_tail = ( shmem_int_atomic_fetch_inc ( &cpr_resrv_queue_tail, i)) % MAX_CARRIER_QSIZE;
                shmem_putmem (&cpr_resrv_queue[q_tail], (void *) carr, 1 * sizeof(cpr_resrv_carrier), i);

                // TEST purpose:
                posted_resrv++;
                // printf("RESERVE carrier posted to pe %d with qtail=%d from pe %d\n", i, q_tail, pe_num);
            }
            // update hashtable
            break;

        case SPARE_PE:
            // waiting to receive the first reservation request in the queue:
            while ( cpr_resrv_queue_head >= cpr_resrv_queue_tail )
            {
                printf("%d stuck in 1st reserve loop\n", me);
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 10000;
                nanosleep(&ts, NULL);
            }
            // printf("RESERVING from a SPARE=%d:\treading %d carriers\n", pe_num, cpr_resrv_queue_tail - cpr_resrv_queue_head);
            while (cpr_resrv_queue_head < cpr_resrv_queue_tail)
            {
                printf("%d stuck in 2nd reserve loop\n", me);
                // TEST Purpose:
                read_resrv++;
                // head and tail might overflow the int size... add code to check
                carr = &cpr_resrv_queue[(cpr_resrv_queue_head % MAX_CARRIER_QSIZE)];
                cpr_resrv_queue_head ++;
                cpr_table_size[ carr-> pe_num] ++;
                // TODO: I should reserve count/1000+1 carriers
                cpr_checkpoint_table[carr-> pe_num] =
                            (cpr_check_carrier *) realloc (cpr_checkpoint_table[carr-> pe_num], 
                                        cpr_table_size[carr-> pe_num] * sizeof(cpr_check_carrier));

                // Preparing the meta data of this piece of checkpoint for later
                // e.g: later if they want to checkpoint with id=5, I lookup for id=5 which
                            // I have assigned here:
                cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1].id = carr -> id;
                cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1].adr = carr -> adr;
                cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1].count = carr -> count;
                cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1].pe_num = carr -> pe_num;
                //USELESS: cpr_checkpoint_table[carr-> pe_num][cpr_table_size[carr-> pe_num]-1] =
                //            (void *) malloc (carr->count * sizeof(int));
                // TODO: update the hash table. I'm assuming id = index here
            }
            // printf("***at the end SPARE=%d:\thas %d carriers left\n", pe_num, cpr_resrv_queue_tail - cpr_resrv_queue_head);
            //return FAILURE;     // if SPAREs are not participated in code, they won't call reserve
            break;

        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }
    return SUCCESS;
}

// for now, assuming we are checkpointing ints
int shmem_cpr_checkpoint ( int id, int* mem, int count, int pe_num )
{
    /* TO DO:
    lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        Spare: cpr_checkpoint_table[pe_num][index]
    */
    // TEST Purpose:
    called_check++;

    cpr_check_carrier *carr = (cpr_check_carrier *) malloc ( sizeof (cpr_check_carrier) ); 
    int i, q_tail;
    int npes = cpr_num_active_pes + cpr_num_spare_pes;

    // for now, I assume id = index
    switch (cpr_pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            for ( i=0; i < count; ++i )
                cpr_shadow_mem[id].data[i] = mem[i];

            // TODO: if count > 1000, I have to send (count/1000)+1 carriers
            carr->id = id;
            carr->count = count;
            carr->pe_num = pe_num;
            carr->adr = mem;

            for ( i=0; i < count; ++i )
                carr -> data[i] = mem[i];

            //printf("CHPING from an ORIGINAL:\tid = %d,\tcount = %d, from pe = %d\n", id, count, pe_num);
            
            // should reserve a place on all spare PEs
            // and update the cpr_table_size of all spare PEs
            for ( i= cpr_first_spare; i < npes; ++i)
            {
                // TEST Purpose:
                posted_check++;
                // shmem_int_atomic_fetch_inc returns the amount before increment
                q_tail = ( shmem_int_atomic_fetch_inc (&cpr_check_queue_tail, i)) % MAX_CARRIER_QSIZE;
                // TEST:
                q_head = ( shmem_int_atomic_fetch (&cpr_check_queue_head, i)) % MAX_CARRIER_QSIZE; 
                printf("%d original putting to %d with qhead=%d, qtail=%d, with id=%d, count=%d\n", me, i, q_head, q_tail, id, count);

                shmem_putmem (&cpr_check_queue[q_tail], carr, 1 * sizeof(cpr_check_carrier), i);
                //printf("CHP carrier posted to pe %d with qtail=%d from pe %d\n", i, q_tail, pe_num);
            }
            // update hashtable
            break;

        case SPARE_PE:
            printf("%d spare is entering chp with head=%d tail=%d\n", me, cpr_check_queue_head, cpr_check_queue_tail);
            // First, we need to check reservation queue is empty. if not, call reservation
            if ( cpr_resrv_queue_head < cpr_resrv_queue_tail )
            {
                //printf("*** entered reservation from checkpointing from pe=%d with %d carriers***\n", pe_num, cpr_resrv_queue_tail-cpr_resrv_queue_head);
                shmem_cpr_reserve(id, mem, count, pe_num);
            }
            // waiting to receive the first checkpointing request in the queue:
            while ( cpr_check_queue_head >= cpr_check_queue_tail )
            {
                //printf("%d is stuck in 1st while with head=%d tail=%d\n", me, cpr_check_queue_head, cpr_check_queue_tail);
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 10000;
                nanosleep(&ts, NULL);
            }
            //printf("CHPING from a SPARE=%d:\treading %d carriers\n", pe_num, cpr_check_queue_tail - cpr_check_queue_head);
            while (cpr_check_queue_head < cpr_check_queue_tail)
            {
                //printf("%d is stuck in 2nd while\n", me);
                // TEST:
                read_check++;
                // head and tail might overflow the int size... add code to check
                carr = &cpr_check_queue[(cpr_check_queue_head % MAX_CARRIER_QSIZE)];
                cpr_check_queue_head ++;
                
                // TEST:
                //if ( pe_num == 8 )
                //    printf("for PE=%d carrier: id=%d, count=%d, pe=%d\n", pe_num, carr->id, carr->count, carr->pe_num);

                for ( i=0; i< carr-> count; ++i)
                {
                    cpr_checkpoint_table[carr-> pe_num][carr-> id].data[i] = carr-> data[i];
                    // TEST:
                    //if ( pe_num == 8 )
                    //    printf("***cpr_checkpoint_table[%d][%d].data[%d]=%d\n\n", carr-> pe_num, carr-> id, i, cpr_checkpoint_table[carr-> pe_num][carr-> id].data[i]);
                }
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


int shmem_cpr_restore ( int dead_pe, int me )
{
    /* TO DO:
    lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        spare: cpr_checkpoint_table[dead_pe][index]
                update the cpr_pe and next_pe
    */
    int i, j;
    cpr_check_carrier *carr;

    if ( me != dead_pe )
    {
        switch (cpr_pe_type)
        {
            case ORIGINAL_PE:
            case RESURRECTED_PE:
                for ( i=0; i<cpr_shadow_mem_size; ++i)
                {
                    *carr = cpr_shadow_mem[i];
                    for ( j=0; j < carr->count; ++j )
                    {
                        *((carr->adr)+j) = carr->data[j];
                    }
                }
                break;

            case SPARE_PE:
                // First, if there is any checkpoint remaining in the queue, should be checkpointed
                if ( cpr_check_queue_head < cpr_check_queue_tail )
                {
                    //printf("*** entered checkpointing from restore from pe=%d with %d carriers***\n", pe_num, cpr_check_queue_tail-cpr_check_queue_head);
                    shmem_cpr_checkpoint(0, NULL, 0, me);
                }
                // The first spare replaces the dead PE
                if ( me == cpr_first_spare )
                {
                    cpr_pe_type = RESURRECTED_PE;
                    cpr_shadow_mem = (cpr_check_carrier *) malloc ( cpr_table_size[dead_pe] * sizeof(cpr_check_carrier));
                    for ( i=0; i < cpr_table_size[dead_pe]; ++i )
                    {
                        cpr_shadow_mem[i] = cpr_checkpoint_table[dead_pe][i];
                        // check if the variable is in memory region of symmetrics
                        // if ( symmetric )
                        // translate the address, new adr
                        for ( j=0; j < cpr_shadow_mem[i].count; ++j )
                        {
                            //*(adr+j) = cpr_shadow_mem[i].data[j];
                        }
                        // else, what?
                    }
                }
                break;

            default:
            // nothing here for now
                break;
        }
    }

    return SUCCESS;
}

int main ()
{
    int spes;
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

    success_init = shmem_cpr_init(me,npes, spes, COLLECTIVE_CHECKPOINT);
    //if ( me == 0 )
    //    printf ("init is %d\n", success_init);
    //printf("I am %d with cpr_pe_type= %d\n", me, cpr_pe_type);

    array_size = 10 + me;
    a = (int *) malloc((array_size)*sizeof(int));
    for ( i=0; i<array_size; ++i)
        a[i] = me;

    //printf("PE %d: array of size:%d\n", me, array_size);

    // not sure if this is necessary here
    shmem_barrier_all ();

    shmem_cpr_reserve(0, &i, 1, me);
    shmem_cpr_reserve(1, a, array_size, me);

    shmem_barrier_all();
    
    for ( i=0; i<40; ++i )
    {
        if ( i%10 == 0)
        {
            shmem_cpr_checkpoint(0, &i, 1, me);
            //if ( cpr_pe_type == SPARE_PE)
            //    printf("PE %d is finished the 1st checkpointing at iter=%d\n", me, i);
            shmem_cpr_checkpoint(1, a, array_size, me);
            //if ( cpr_pe_type == SPARE_PE)
            //    printf("PE %d is finished the 2nd checkpointing at iter=%d\n", me, i);
        }

        for ( j=0; j<array_size; ++j)
            a[j] ++;
        /*
        if ( i == 25 ){
            shmem_cpr_rollback();
            if ( me == 0)
                printf("AFTER ROLLBACK:\n");
            printf("PE %d: \t i=%d \t a[0]=%d\n", me, i, a[0]);
        }*/
    }

    if ( cpr_pe_type == SPARE_PE)
        printf("PE %d is finished checkpointing before barrier\n", me);

    shmem_barrier_all ();

    if ( cpr_pe_type == SPARE_PE)
        printf("PE %d is finished checkpointing\n", me);

    if ( me == 8 )
    {
        cpr_check_carrier *carr;
        printf("active PEs=%d\n***\n", cpr_num_active_pes);
        for ( i=0; i < cpr_num_active_pes; ++i )
        {
            //printf("for PE=%d, we have %d carriers\n", i, cpr_table_size[i]);
            
            for ( j=0; j < cpr_table_size[i] - 1; ++j )
            {
                *carr = cpr_checkpoint_table[i][j];
                //printf("for PE=%d carrier=%d: id=%d, count=%d, pe=%d\n", i, j, carr->id, carr->count, carr->pe_num);
                int k;
                /*
                for ( k=0; k < carr->count; ++k)
                    printf("%d  ", carr->data[k]);
                printf("\n------------------\n");
                */
            }
        }
    }

    //for ( j=0; j<npes; ++j )
    //    if ( me == j )
    //    {
    //        printf("I am =%d, called %d reservs and %d checks,\t posted %d reservs and %d checks,\t and read %d reservs and %d checks\n", me, called_resrv, called_check, posted_resrv, posted_check, read_resrv, read_check);
    //    }
    /*
    if ( me == 0)
        printf("\nFINALLY\n");
    printf("PE %d: \t a[0]=%d\n", me, a[0]);
    */
    shmem_finalize();

    return 0;
}