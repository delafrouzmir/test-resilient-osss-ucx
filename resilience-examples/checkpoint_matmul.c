#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <shmem.h>


#define CPR_ORIGINAL_PE     0
#define CPR_SPARE_PE        1
#define CPR_MSPE            2
#define CPR_RESURRECTED_PE  3
#define CPR_DEAD_PE         4

#define CPR_ACTIVE_ROLE     0
#define CPR_STORAGE_ROLE    1
#define CPR_DORMANT_ROLE    2

#define SUCCESS     0
#define FAILURE     1

#define CPR_NO_CHECKPOINT           0
#define CPR_MANY_COPY_CHECKPOINT    1
#define CPR_TWO_COPY_CHECKPOINT     2

#define CPR_CARR_DATA_SIZE          100
#define CPR_STARTING_QUEUE_LEN      100
#define CPR_STARTING_TABLE_SIZE     16


///////////////////////////
/****                 ****/
/*    variable defs      */
/****                 ****/
///////////////////////////

// TO DO: add a struct to hold the info of failure, like type and reason and where it happened

// names starting with shmem_cpr_ are reserved for functions
// names starting with cpr_ are reserved for variable declarations
typedef struct resrv_carrier
{
    int id;                     // ID of memory part that is being reserved
    unsigned long *adr;                   // Address of memory part that is being reserved
    int count;                  // number of data items that needs to be stored
    int pe_num;                 // the PE that asked for a reservation or checkpoint
    int is_symmetric;           // if this request is to checkpoint symmetric or private data
    int rand_num;               // rand_num to wait for arrival of new carriers in queues
} cpr_rsvr_carrier;

typedef struct check_carrier cpr_check_carrier;
struct check_carrier
{
    int id;                     // ID of memory part that is being checkpointed
    unsigned long *adr;                   // Address of memory part that is being checkpointed
    // TO DO: maybe if change from count to size=count*sizeof(data),
    // it can be generalized to all 
    int count;                  // number of data items that needs to be stored
    unsigned long data[CPR_CARR_DATA_SIZE];   // an array of data that will be stored
    int pe_num;                 // the PE that asked for a reservation or checkpoint
    int is_symmetric;           // if this request is to checkpoint symmetric or private data
    int offset;
    int rand_num;               // rand_num to wait for arrival of new carriers in queues
};

// Part 1: necessary for keeping checkpoints in shadow mem
cpr_check_carrier ***cpr_shadow_mem;    // a PE's own copy of its checkpoints, an array of pointers to carriers
int cpr_shadow_mem_size, cpr_shadow_mem_tail;

// Part 2: necessary for keeping checkpoints in checkpoint table
cpr_check_carrier ****cpr_checkpoint_table; // a spare PE or MSPE's copy of all active PE's checkpoints == an array of all their shadow mems
int *cpr_table_size, *cpr_table_tail;

// Part 3: queues necessary for exchange of data when reserving or checkpointing
cpr_check_carrier cpr_check_queue[CPR_STARTING_QUEUE_LEN];
int cpr_check_queue_head, cpr_check_queue_tail, cpr_sig_check;
int* check_randomness;

cpr_rsvr_carrier cpr_resrv_queue[CPR_STARTING_QUEUE_LEN];
int cpr_resrv_queue_head, cpr_resrv_queue_tail, cpr_sig_rsvr;
int* rsrv_randomness;

// Part 4: keeping the number of different pe types
//int cpr_first_mspe, cpr_second_mspe, cpr_first_spare;
int cpr_num_spare_pes, cpr_num_active_pes, cpr_num_storage_pes;
int *cpr_storage_pes;

// Part 5: keeping checkpointing info in every PE
int cpr_pe_role, cpr_pe_type;
int cpr_checkpointing_mode, cpr_sig, cpr_start;
/* TO DO:
*  change the cpr_pe from array to hashtable
*  since we want to know both which PE has replaced a dead PE (= the old PE num of a resurrected PE)
*  as well as know what PE a spare PE has replaces (= the new PE num of a previously spare PE)
*/
int *cpr_pe, *cpr_all_pe_type, *cpr_all_pe_role, *cpr_replaced;

// Part 6: delarrations for the application and testing
int me, npes;
int called_check, called_resrv, posted_check, posted_resrv, read_check, read_resrv;


///////////////////////////
/****                 ****/
/*    function defs      */
/****                 ****/
///////////////////////////

// void delay(double dly){
//     /* save start time */
//     const time_t start = time(NULL);

//     time_t current;
//     do{
//         /* get current time */
//         time(&current);
//         /* break loop when the requested number of seconds have elapsed */
//     }while(difftime(current, start) < dly);
// }

void shmem_cpr_set_pe_type (int me, int npes, int spes, int cpr_mode)
{
    // spes is checked before, we know it is a positive int

    // instead of pe number i, we should use cpr_pe[i]
    // e.g: shmem_int_put(dest, &source, nelems, cpr_pe[j]);

    int i;

    for ( i=0; i<npes; ++i )
        cpr_pe[i] = i;

    for ( i=0; i<cpr_num_active_pes; ++i)
    {
        // PEs 0 to npes-spes-1 are originals in any case
        cpr_all_pe_type[i] = CPR_ORIGINAL_PE;
        cpr_all_pe_role[i] = CPR_ACTIVE_ROLE;
    }
    
    // PEs 0 to npes-spes-1 are originals in any case
    if ( me >= 0 && me < npes - spes )
    {
        cpr_pe_type = CPR_ORIGINAL_PE;
        cpr_pe_role = CPR_ACTIVE_ROLE;
    }

    switch (cpr_mode)
    {
        case CPR_MANY_COPY_CHECKPOINT:
            /*
            * PEs 0 to npes-spes-1 are originals
            * the rest are storages
            */

            cpr_num_storage_pes = spes;
            
            if ( me >= npes - spes )
            {
                cpr_pe_type = CPR_SPARE_PE;
                cpr_pe_role = CPR_STORAGE_ROLE;
            }
            
            for ( i = npes - spes ; i < npes; ++i )
            {
                cpr_all_pe_type[i] = CPR_SPARE_PE;
                cpr_all_pe_role[i] = CPR_STORAGE_ROLE;
                cpr_storage_pes[i-(npes-spes)] = i;
            }

            break;

        case CPR_TWO_COPY_CHECKPOINT:
            /*
            * PEs 0 to npes-spes-1 are originals
            * PEs npes-1 and npes-spes are CPR_MSPEs and sotrages (to probably avoid being on the same node)
            * the rest are spares, and dormant
            */
            
            if ( spes > 2)
            {
                cpr_num_storage_pes = 2;
                cpr_storage_pes[0] = npes - spes;
                cpr_storage_pes[1] = npes - 1;

                if ( me == npes-1 || me == npes - spes )
                {
                    cpr_pe_type = CPR_MSPE;
                    cpr_pe_role = CPR_SPARE_PE;
                }
                else if ( me > npes - spes )
                {
                   cpr_pe_type = CPR_SPARE_PE;
                   cpr_pe_role = CPR_DORMANT_ROLE;
                }
                
                cpr_all_pe_type[npes-1] = CPR_MSPE;
                cpr_all_pe_type[npes - spes] = CPR_MSPE;
                cpr_all_pe_role[npes-1] = CPR_STORAGE_ROLE;
                cpr_all_pe_role[npes - spes] = CPR_STORAGE_ROLE;
                
                for ( i = npes-spes +1 ; i < npes-1; ++i )
                {
                    cpr_all_pe_type[i] = CPR_SPARE_PE;
                    cpr_all_pe_role[i] = CPR_DORMANT_ROLE;
                }
            }

            else // spes == 1 || spes == 2
            {
                // there will be no spare PEs, only 1 or 2 CPR_MSPEs
                if ( me >= npes - spes )
                {
                    cpr_pe_type = CPR_MSPE;
                    cpr_pe_role = CPR_STORAGE_ROLE;
                }

                cpr_all_pe_type[npes-1] = CPR_MSPE;
                cpr_all_pe_role[npes-1] = CPR_STORAGE_ROLE;

                if ( spes == 1 )
                {
                    cpr_storage_pes[0] = npes-1;
                    cpr_num_storage_pes = 1;
                }
                else    // spes == 2
                {
                    cpr_num_storage_pes = 2;
                    cpr_all_pe_type[npes-2] = CPR_MSPE;
                    cpr_all_pe_role[npes-2] = CPR_STORAGE_ROLE;
                    cpr_storage_pes[0] = npes-2;
                    cpr_storage_pes[1] = npes-1;
                }
            }
            
            break;

        case CPR_NO_CHECKPOINT:
        default:
            return;
    }
    
}


int shmem_cpr_init (int me, int npes, int spes, int mode)
{
    /*
    * Incorrect input
    */
    if ( spes == 0 || spes > npes-1 || me < 0 || me >= npes )
    {
        cpr_checkpointing_mode = CPR_NO_CHECKPOINT;
        return FAILURE;
    }
    if ( mode != CPR_NO_CHECKPOINT && mode != CPR_MANY_COPY_CHECKPOINT && mode != CPR_TWO_COPY_CHECKPOINT )
    {
        cpr_checkpointing_mode = CPR_NO_CHECKPOINT;
        return FAILURE;
    }

    // Setting up numbers of active and spare PEs
    // and arrays containing info on types and roles of PEs

    cpr_num_spare_pes = spes;
    cpr_num_active_pes = npes - spes;
    cpr_pe = (int *) malloc (npes * sizeof(int));
    cpr_replaced = (int *) malloc (npes * sizeof(int));
    cpr_all_pe_type = (int *) malloc (npes * sizeof(int));
    cpr_all_pe_role = (int *) malloc (npes * sizeof(int));
    cpr_storage_pes = (int *) malloc (spes * sizeof (int));

    // add an if for different checkpointing mode here
    switch (mode)
    {
        case CPR_NO_CHECKPOINT:
            cpr_checkpointing_mode = CPR_NO_CHECKPOINT;
            // TO DO: free up symmteric things used for checkpointing
            return SUCCESS;
            break;
        
        case CPR_MANY_COPY_CHECKPOINT:
            cpr_checkpointing_mode = CPR_MANY_COPY_CHECKPOINT;
            break;

        case CPR_TWO_COPY_CHECKPOINT:
        default:
            cpr_checkpointing_mode = CPR_TWO_COPY_CHECKPOINT;
            break;
    }
        
    shmem_cpr_set_pe_type (me, npes, spes, cpr_checkpointing_mode);

    // * ORIGINAL or RESURRECTED PEs have a shadow_mem for chckpointing their own data
    // * SPARE PEs:
    //     ** if in MANY_COPY mode: have a checkpoint_table which is a copy of all
    //         *** the ORIGINAL or RESURRECTED PEs' shadow_mem's
    //     ** if in TWO-COPY mode: have a shadow_mem and checkpoint_table to be prepared later
    // * CPR_MSPEs:
    //     ** if in MANY_COPY mode: wrong type, error
    //     ** if in TWO-COPY mode: have a checkpoint_table which is a copy of all
    //         *** the ORIGINAL or RESURRECTED PEs' shadow_mem's
    int i, j;

    cpr_shadow_mem_tail = 0;
    cpr_shadow_mem_size = CPR_STARTING_TABLE_SIZE;
    cpr_shadow_mem = (cpr_check_carrier ***) malloc(cpr_shadow_mem_size * sizeof(cpr_check_carrier **) );
    
    switch (cpr_pe_role)
    {
        case CPR_STORAGE_ROLE:
            // table_size doubles every time we ask for more space
            // table_tail shows the last place reserved
            cpr_checkpoint_table = (cpr_check_carrier ****) malloc (cpr_num_active_pes * sizeof(cpr_check_carrier ***));
            cpr_table_size = (int *) malloc(cpr_num_active_pes * sizeof(int *));
            cpr_table_tail = (int *) malloc(cpr_num_active_pes * sizeof(int *));
            rsrv_randomness = (int *) malloc(CPR_STARTING_QUEUE_LEN * sizeof(int));
            check_randomness = (int *) malloc(CPR_STARTING_QUEUE_LEN * sizeof(int));

            for (i=0; i<cpr_num_active_pes; ++i)
            {
                cpr_table_size[i] = CPR_STARTING_TABLE_SIZE;
                cpr_table_tail[i] = 0;
                cpr_checkpoint_table[i] = (cpr_check_carrier ***) malloc (cpr_table_size[i] * sizeof(cpr_check_carrier **));
            }

            break;
        
        case CPR_ACTIVE_ROLE:
        case CPR_DORMANT_ROLE:   
        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }

    srand(me); // to make PEs generate different rands than each other

    shmem_barrier_all();
    cpr_start = 1;

    return SUCCESS;
}

int shmem_cpr_pe_num ( int pe_num)
{
    int npes = cpr_num_active_pes + cpr_num_spare_pes;
    if ( pe_num >= 0 && pe_num < npes )
        return cpr_pe[pe_num];
    return -1;
}

int shmem_cpr_is_new_reservation (int id)
{
    // TO DO: print warnings for wrong id, mem addr, or like that
    // lookup the id in hashtable, if repetetive, return false;
    
    // Only originals or resurrected call this function
    if ( cpr_pe_type != CPR_ORIGINAL_PE && cpr_pe_type != CPR_RESURRECTED_PE )
        return 0;

    // now we have made sure this PE has a shadow mem
    if ( id < cpr_shadow_mem_tail && cpr_shadow_mem[id] != NULL )
       return 0;

    return 1;
}

void shmem_cpr_copy_carrier ( cpr_rsvr_carrier *frst, cpr_check_carrier *scnd )
{
    if ( frst == NULL || scnd == NULL )
    {
        printf("error. one is null\n");
        return;
    }
    scnd -> id = frst -> id;
    scnd -> adr = frst -> adr;
    scnd -> count = frst -> count;
    scnd -> pe_num = frst -> pe_num;
    scnd -> is_symmetric = frst -> is_symmetric;
}

int shmem_cpr_is_reserved (int id, unsigned long *mem, int pe_num)
{
    switch(cpr_pe_type)
    {
        case CPR_ORIGINAL_PE:
        case CPR_RESURRECTED_PE:
            // TO DO: if ( id --> hash)
            return 1;
            break;

        case CPR_MSPE:
            if ( cpr_checkpointing_mode == CPR_TWO_COPY_CHECKPOINT )
            {
                // TO DO: if ( id --> hash )
                return 1;
            }
            else    // this function should not have been called
                return 0;
            break;

        case CPR_SPARE_PE:
            if ( cpr_checkpointing_mode == CPR_MANY_COPY_CHECKPOINT )
            {
                // TO DO: if ( id --> hash )
                return 1;
            }
            else    // this function does not matter
                return 0;
            break;

        default:
            return 0;
            break;
    }
}

int shmem_cpr_reserve (int id, unsigned long * mem, int count, int pe_num)
{
    /* TO DO:
    1- create a hash table for id s and the index in cpr_shadow_mem or cpr_checkpoint_table
    2- generalize from int to type ( ==> type * mem)
    6- what type of sync is needed when an original calls this function?
    8- hashtable: id should be unique. look up id. if it already exists, error for user
    9- IMPORTANT: what is pe is resurrected and pe_num is not what we have in chp_table?
    e.g: PE 9 replaces PE 5 while there are only 6 original PEs. chp_table has 6 rows then.
    what if we reach the 9th?
    should it be the user's responsibility or ours?!
    if ours, then maybe hashtable for pe numbers can help
    */
    if ( cpr_pe_type == CPR_DEAD_PE || pe_num < 0 )
        return FAILURE;

    cpr_rsvr_carrier *carr = (cpr_rsvr_carrier *) malloc ( sizeof (cpr_rsvr_carrier) ); 
    int i, q_tail, space_needed;
    // TO DO: could/should this npes change through the program?
    int npes = cpr_num_active_pes + cpr_num_spare_pes;


    // for now, I assume id == index or id == cpr_shadow_mem_tail-1
    switch (cpr_pe_role)
    {
        case CPR_ACTIVE_ROLE:
            if ( shmem_cpr_is_new_reservation (id) )
            {
                carr->id = id;
                carr->adr = mem;
                carr->count = count;
                carr->pe_num = pe_num;
                carr->rand_num = rand();
                // check if mem is symmetric or not
                carr->is_symmetric = shmem_addr_accessible(mem, cpr_storage_pes[0]);

                // calculating ceiling of num of carriers needed in chp table
                space_needed = 1+ (carr->count-1) / CPR_CARR_DATA_SIZE;

                if ( cpr_shadow_mem_tail + 1 > cpr_shadow_mem_size )
                {

                    cpr_shadow_mem_size *= 2;
                    cpr_shadow_mem = (cpr_check_carrier ***) realloc (cpr_shadow_mem,
                            cpr_shadow_mem_size * sizeof(cpr_check_carrier **) );
                }
                cpr_shadow_mem_tail ++;

                // updating the shadow mem with the reservation request:
                cpr_shadow_mem[cpr_shadow_mem_tail-1] = (cpr_check_carrier **) malloc ( space_needed* sizeof(cpr_check_carrier*));
                
                for ( i=0; i<space_needed; ++i )
                {
                    cpr_shadow_mem[cpr_shadow_mem_tail-1][i] =
                        (cpr_check_carrier *) malloc ( 1* sizeof(cpr_check_carrier));
                    shmem_cpr_copy_carrier (carr, cpr_shadow_mem[cpr_shadow_mem_tail-1][i]);
                }

                // should reserve a place on all storage PEs
                // and update the cpr_table_tail of all spare PEs
                for ( i=0; i < cpr_num_storage_pes; ++i )
                {
                    if ( cpr_all_pe_type[cpr_storage_pes[i]] != CPR_DEAD_PE )
                    {
                        q_tail = ( shmem_atomic_fetch_inc ( &cpr_resrv_queue_tail, cpr_storage_pes[i])) % CPR_STARTING_QUEUE_LEN;
                        
                        shmem_putmem (&cpr_resrv_queue[q_tail], (void *) carr, 1 * sizeof(cpr_rsvr_carrier), cpr_storage_pes[i]);
                        
                        if ( shmem_atomic_fetch ( &cpr_sig_rsvr, cpr_storage_pes[i]) == 0 )
                            shmem_atomic_set( &cpr_sig_rsvr, 1, cpr_storage_pes[i]);
                    }
                }
            }
            break;

        case CPR_STORAGE_ROLE:
            // Waiting to probably receive the first reservation request in the queue:
            // Because of collectives and barriers, spare PEs cannot busy wait for receiving
            // carriers.
            // They also might not be able to read all carriers sent to them in 1 function call
            // (because they arrive at different times)
            // during the same function call and read them in the next function call

            if ( cpr_pe_type != CPR_DEAD_PE )
            {
                shmem_wait_until ( &cpr_sig_rsvr, SHMEM_CMP_GT, 0);
                
                while (cpr_resrv_queue_head < cpr_resrv_queue_tail)
                {
                    // almost making sure the carrier has arrived
                    shmem_wait_until(&cpr_resrv_queue[(cpr_resrv_queue_head % CPR_STARTING_QUEUE_LEN)].rand_num,
                        SHMEM_CMP_NE, rsrv_randomness[cpr_resrv_queue_head % CPR_STARTING_QUEUE_LEN]);
                    rsrv_randomness[cpr_resrv_queue_head % CPR_STARTING_QUEUE_LEN] = 
                        cpr_resrv_queue[(cpr_resrv_queue_head % CPR_STARTING_QUEUE_LEN)].rand_num;
                    
                    // TO DO: head and tail might overflow the int size... add code to check
                    *carr = cpr_resrv_queue[(cpr_resrv_queue_head % CPR_STARTING_QUEUE_LEN)];

                    // if ( me == 8 )
                    // {
                    //     printf("rsrv_Carr[%d].pe=%d id=%d symm=%d count=%d rand=%d\n",
                    //         cpr_resrv_queue_head, carr->pe_num, carr->id,
                    //         carr->is_symmetric, carr->count, carr->rand_num);
                    // }

                    cpr_resrv_queue_head ++;
                    
                    // calculating ceiling of num of carriers needed in chp table
                    space_needed = 1+ (carr->count-1) / CPR_CARR_DATA_SIZE;

                    if ( cpr_table_tail[carr-> pe_num] + 1 > cpr_table_size[ carr-> pe_num] )
                    {
                        cpr_table_size[ carr-> pe_num] *= 2;
                        cpr_checkpoint_table[carr-> pe_num] =
                                (cpr_check_carrier ***) realloc (cpr_checkpoint_table[carr-> pe_num], 
                                    cpr_table_size[carr-> pe_num] * sizeof(cpr_check_carrier **));
                    }

                    // Preparing the meta data of this piece of checkpoint for later
                    // e.g: later if they want to checkpoint with id=5, I lookup for id=5 which
                                // I have assigned here:                
                    cpr_checkpoint_table[carr-> pe_num][cpr_table_tail[carr-> pe_num]] = 
                        (cpr_check_carrier **) malloc ( space_needed * sizeof(cpr_check_carrier*));
                    for ( i=0; i<space_needed; ++i )
                    {
                        cpr_checkpoint_table[carr-> pe_num][cpr_table_tail[carr-> pe_num]][i] = 
                            (cpr_check_carrier *) malloc ( 1 * sizeof(cpr_check_carrier));
                        shmem_cpr_copy_carrier (carr, cpr_checkpoint_table[carr-> pe_num][cpr_table_tail[carr-> pe_num]][i]);
                    }
                    
                    cpr_table_tail[ carr-> pe_num] ++;
                    // TODO: update the hash table. I'm assuming id = index here
                }
                cpr_sig_rsvr = 0;
            }
            break;

        default:
            // Nothing here for now, but it may change "if" init is called for subtitute PEs
            break;
    }
    return SUCCESS;
}

// for now, assuming we are checkpointing ints
int shmem_cpr_checkpoint ( int id, unsigned long* mem, int count, int pe_num )
{
    /* TO DO:
    1- lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        Spare or MSPE: cpr_checkpoint_table[pe_num][index]
    2- check if this request for checkpoint has a reservation first
    */

    if ( cpr_pe_type == CPR_DEAD_PE || pe_num < 0 )
        return FAILURE;

    cpr_check_carrier *carr = (cpr_check_carrier *) malloc ( sizeof (cpr_check_carrier) ); 
    int i, j, k, q_tail, space_needed, last_data;
    //TEST
    int q_head;
    int npes = cpr_num_active_pes + cpr_num_spare_pes;

    // first we have to check if this data has reserved a place before
    if ( shmem_cpr_is_reserved (id, mem, pe_num) )
    {
        // TO DO: change to hash table. for now, I assume id = index
        switch (cpr_pe_role)
        {
            case CPR_ACTIVE_ROLE:
                // TO DO: differential checkpointing? what if count is smaller than the count that was reserved?

                space_needed = 1+ (count-1) / CPR_CARR_DATA_SIZE;

                for ( i=0; i < count; ++i )
                    cpr_shadow_mem[id][i/CPR_CARR_DATA_SIZE] -> data[i % CPR_CARR_DATA_SIZE] = mem[i];

                carr->id = id;
                carr->count = count;
                carr->pe_num = pe_num;
                carr->adr = mem;
                // if ( me == 0 )
                //     printf("me=%d 1st\n", me);
                for ( i= 0; i < cpr_num_storage_pes; ++i)
                {
                    if ( cpr_all_pe_type[cpr_storage_pes[i]] != CPR_DEAD_PE )
                    {
                        // if ( me == 0 )
                        //     printf("me=%d 2nd\n", me);
                        for ( j=0; j < space_needed; ++j )
                        {
                            carr -> offset = j * CPR_CARR_DATA_SIZE;
                            carr -> rand_num = rand();

                            last_data = (j == space_needed-1) ? 
                                count - (j*CPR_CARR_DATA_SIZE) : CPR_CARR_DATA_SIZE;

                            // if ( me == 0 )
                            //     printf("me=%d 3rd\n", me);

                            for ( k=0; k < last_data; ++k )
                                carr -> data[k] = mem[k + j*CPR_CARR_DATA_SIZE];

                            // shmem_atomic_fetch_inc returns the amount before increment
                            q_tail = ( shmem_atomic_fetch_inc (&cpr_check_queue_tail, cpr_storage_pes[i])) % CPR_STARTING_QUEUE_LEN;

                            shmem_putmem (&cpr_check_queue[q_tail], carr, 1 * sizeof(cpr_check_carrier), cpr_storage_pes[i]);

                            // if ( me == 0 )
                            //     printf("me=%d 4th\n", me);
                        }

                        if ( shmem_atomic_fetch ( &cpr_sig_check, cpr_storage_pes[i]) == 0 )
                            shmem_atomic_set( &cpr_sig_check, 1, cpr_storage_pes[i]);
                        
                    }
                }
                // update hashtable
                break;

            case CPR_STORAGE_ROLE:

                // First, we need to check reservation queue is empty. if not, call reservation
                if ( cpr_pe_type != CPR_DEAD_PE )
                {
                    if ( cpr_resrv_queue_head < cpr_resrv_queue_tail )
                        shmem_cpr_reserve(id, mem, count, pe_num);
                    // if ( me == 8 )
                    //     printf("me=%d 5th\n", me);
                    // waiting to receive the first checkpointing request in the queue:
                    shmem_wait_until ( &cpr_sig_check, SHMEM_CMP_GT, 0);
                    // if ( me == 8 )
                    //     printf("me=%d 6th\n", me);
                    while (cpr_check_queue_head < cpr_check_queue_tail)
                    {
                        // almost making sure the carrier has arrived
                        shmem_wait_until(&cpr_check_queue[(cpr_check_queue_head % CPR_STARTING_QUEUE_LEN)].rand_num,
                            SHMEM_CMP_NE, check_randomness[cpr_check_queue_head % CPR_STARTING_QUEUE_LEN]);
                        check_randomness[cpr_check_queue_head % CPR_STARTING_QUEUE_LEN] = 
                            cpr_check_queue[(cpr_check_queue_head % CPR_STARTING_QUEUE_LEN)].rand_num;
                        // if ( me == 8 )
                        //     printf("me=%d 7th\n", me);
                        // head and tail might overflow the int size... add code to check
                        carr = &cpr_check_queue[(cpr_check_queue_head % CPR_STARTING_QUEUE_LEN)];
                        // if ( me == 8 && carr->count == 1)
                        // {
                        //     printf("check_Carr[%d].pe=%d id=%d symm=%d count=%d rand=%d offset=%d\n%lu\n",
                        //         cpr_check_queue_head, carr->pe_num, carr->id,
                        //         carr->is_symmetric, carr->count, carr->rand_num,
                        //         carr->offset, carr->data[0]);

                        // }
                        // if ( me == 8 && carr->count == 10)
                        // {
                        //     printf("check_Carr[%d].pe=%d id=%d symm=%d count=%d rand=%d offset=%d\n%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
                        //         cpr_check_queue_head, carr->pe_num, carr->id,
                        //         carr->is_symmetric, carr->count, carr->rand_num,
                        //         carr->offset, carr->data[0], carr->data[1],
                        //         carr->data[2], carr->data[3], carr->data[4], carr->data[5],
                        //         carr->data[6], carr->data[7], carr->data[8], carr->data[9],
                        //         carr->data[10]);

                        // }
                        cpr_check_queue_head ++;
                        
                        // if ( me == 8 )
                        //     printf("******carr->count=%d, CPR_CARR_DATA_SIZE=%d\n", carr->count, CPR_CARR_DATA_SIZE);

                        if ( carr->count <= CPR_CARR_DATA_SIZE )
                            last_data = carr->count;
                        else
                        {
                            if ( carr->count - carr->offset <= CPR_CARR_DATA_SIZE )
                                last_data = carr->count - carr->offset;
                            else
                                last_data = CPR_CARR_DATA_SIZE;
                        }
                        // if ( me == 8 )
                        //     printf("***last_data=%d\n", last_data);
                        for ( i=0; i< last_data; ++i)
                        {
                            cpr_checkpoint_table[carr-> pe_num][carr-> id][(carr->offset)/CPR_CARR_DATA_SIZE] -> data[i] = carr-> data[i];
                            // if ( me == 8 && carr-> pe_num < 3 )
                            //     printf("cpr_checkpoint_table[%d][%d][%d]->data[%d] = %d\n",
                            //         carr-> pe_num, carr-> id, (carr->offset)/CPR_CARR_DATA_SIZE,
                            //         i, carr-> data[i]);
                        }
                        // if ( me == 8 )
                        //     printf("me=%d 8th\n", me);
                        // I'm assuming id = index here
                    }
                }
                //return FAILURE;     // if SPAREs are not participated in code, they won't call reserve
                break;

            default:
            // nothing here for now
                break;
        }
    }
    
    return SUCCESS;
}

void shmem_cpr_copy_check_table ( int candid, int storage )
{

}

int shmem_cpr_rollback ( int dead_pe, int me )
{
    /* TO DO:
    lookup id in hashtable to get the index
    update the index-th element in
        original or ressurected: shadow mem
        spare: cpr_checkpoint_table[dead_pe][index]
                update the cpr_pe and next_pe
    */
    int i, j, k, chosen_pe, reading_carr, reading_data;
    cpr_check_carrier *carr;

    /* TO DO:
    *  Right now, the strategy to replace a dead PE with the first remaining spare PE
    *  Look into better positioning options:
    *  e.g. replacing every dead PE with a spare from the same node
    */

    if ( me != dead_pe && cpr_all_pe_role[dead_pe] == CPR_ACTIVE_ROLE)
    {
        chosen_pe = cpr_storage_pes[cpr_num_storage_pes-1];

        switch (cpr_pe_role)
        {
            // every Original or Resurrected PE should just rollback to the last checkpoint
            case CPR_ACTIVE_ROLE:
                for ( i=0; i<cpr_shadow_mem_tail; ++i)
                {
                    reading_carr = 1+ (cpr_shadow_mem[i][0]->count-1) / CPR_CARR_DATA_SIZE;
                    for ( k=0; k < reading_carr; ++k )
                    {
                        carr = cpr_shadow_mem[i][k];
                        reading_data = ( k== reading_carr-1 ) ?
                            (carr->count)%CPR_CARR_DATA_SIZE 
                            : CPR_CARR_DATA_SIZE;
                        for ( j=0; j < reading_data; ++j )
                            *((carr->adr)+(carr->offset)+j) = carr->data[j];
                    }
                }
                break;

            // Spare PEs should check first if they have any remaining carriers in checkpoint queues
            case CPR_STORAGE_ROLE:
                // First, if there is any checkpoint remaining in the queue, should be checkpointed
                if ( cpr_check_queue_head < cpr_check_queue_tail )
                    shmem_cpr_checkpoint(0, NULL, 0, me);
                
                // The last spare replaces the dead PE
                if ( me == chosen_pe )
                {
                    cpr_pe_type = CPR_RESURRECTED_PE;
                    cpr_pe_role = CPR_ACTIVE_ROLE;

                    cpr_shadow_mem = (cpr_check_carrier ***) malloc ( cpr_table_size[dead_pe] * sizeof(cpr_check_carrier **));
                    for ( i=0; i < cpr_table_tail[dead_pe]; ++i )
                    {
                        reading_carr = 1+
                            (cpr_checkpoint_table[dead_pe][i][0] -> count-1) / CPR_CARR_DATA_SIZE;
                        
                        cpr_shadow_mem[i] = (cpr_check_carrier **) malloc ( reading_carr * sizeof(cpr_check_carrier*));

                        for ( j=0; j<reading_carr; ++j )
                        {
                            cpr_shadow_mem[i][j] =
                                (cpr_check_carrier *) malloc ( 1 * sizeof(cpr_check_carrier));
                            cpr_shadow_mem[i][j] = cpr_checkpoint_table[dead_pe][i][j];
                            carr = cpr_shadow_mem[i][j];
                            // check if the variable is in memory region of symmetrics
                            if ( carr -> is_symmetric )
                            {
                                reading_data = ( j == reading_carr-1) ?
                                    (carr -> count) % CPR_CARR_DATA_SIZE
                                    : CPR_CARR_DATA_SIZE;

                                for ( k=0; k < reading_data; ++k )
                                    *(carr->adr + carr->offset +j) = carr->data[k];
                            }
                            else
                            {
                                // define an array of tuplets, one addr and one data
                            }
                        }
                    }
                }

                // If we are in 2-copy-mode, we should prepare another storage PE to hold the checkpointing-table
                // Precisely: the last storage PE should copy the table to a spare PE

                // TO DO:
                // 1- check for number of spares left
                // 2- reduce the number of storage/spare PEs if necessary
                // 3- add the new storage PE to the array of storage PEs
                if ( cpr_checkpointing_mode == CPR_TWO_COPY_CHECKPOINT )
                {
                    int candid_storage = -1;
                    for ( i = cpr_num_active_pes; i < npes; ++i )
                        if ( cpr_all_pe_type[i] == CPR_SPARE_PE && cpr_all_pe_role[i] == CPR_DORMANT_ROLE )
                        {
                            candid_storage = i;
                            break;
                        }

                    if ( candid_storage != -1 &&(me == candid_storage || me == cpr_storage_pes[0]) )
                        shmem_cpr_copy_check_table ( candid_storage, cpr_storage_pes[0] );
                }
                break;

            default:
            // nothing here for now
                break;
        }

        cpr_all_pe_role[chosen_pe] = CPR_ACTIVE_ROLE;
        cpr_all_pe_type[chosen_pe] = CPR_RESURRECTED_PE;
        cpr_pe[dead_pe] = -1;
        // cpr_pe[i] shows which PE plays the role of PE_i
        // cpr_replaced[i] shows which PE has replaced PE_i
        cpr_pe[chosen_pe] = dead_pe;
        cpr_replaced[dead_pe] = chosen_pe;

        cpr_all_pe_type[dead_pe] = CPR_DEAD_PE;

        cpr_num_storage_pes --;
    }

    if ( me != dead_pe && cpr_all_pe_role[dead_pe] == CPR_STORAGE_ROLE){

        cpr_pe[dead_pe] = -1;
        cpr_num_storage_pes --;
        cpr_all_pe_type[dead_pe] = CPR_DEAD_PE;

        if ( cpr_checkpointing_mode == CPR_TWO_COPY_CHECKPOINT )
        {
            int candid_storage = -1;
            for ( i = cpr_num_active_pes; i < npes; ++i )
                if ( cpr_all_pe_type[i] == CPR_SPARE_PE && cpr_all_pe_role[i] == CPR_DORMANT_ROLE )
                {
                    candid_storage = i;
                    break;
                }

            if ( candid_storage != -1 &&(me == candid_storage || me == cpr_storage_pes[0]) )
                shmem_cpr_copy_check_table ( candid_storage, cpr_storage_pes[0] );
        }
    }

    return SUCCESS;
}

void mmul(const unsigned long Is, const unsigned long Ks, const unsigned long Js,
          const unsigned long Adist, const unsigned long* A,
          const unsigned long Bdist, const unsigned long* B,
          const unsigned long Cdist, unsigned long* C) {

    unsigned long i, j, k;
    unsigned long a_ik;

    for (i = 0; i < Is; i++) {
        for (k = 0; k < Ks; k++) {
            a_ik = A[i * Adist + k];
            for (j = 0; j < Js; j++) {
                C[i * Cdist + j] += (a_ik * B[k * Bdist + j]);
            }
        }
    }
}


void print_matrix(const unsigned long* mat, const unsigned long Is, const unsigned long Js) {
    for (unsigned long i = 0; i < Is; i++) {
        for (unsigned long j = 0; j < Js; j++)
            printf("%d ", mat[i * Js + j]);
        printf("\n");
    }
}


int main ()
{
    int spes;
    int success_init;
    int i, j, k, l, array_size, first_rollback;
    unsigned long* a, *iter;

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

    success_init = shmem_cpr_init(me, npes, spes, CPR_MANY_COPY_CHECKPOINT);

    iter = (unsigned long *) shmem_malloc(sizeof(unsigned long));

    array_size = 10;
    a = (unsigned long *) shmem_malloc((array_size)*sizeof(unsigned long));
    for ( i=0; i<array_size; ++i)
        a[i] = me;

    // not sure if this is necessary here
    shmem_barrier_all ();

    first_rollback = 0;
    *iter = 0;
    
    shmem_cpr_reserve(0, iter, 1, shmem_cpr_pe_num(me));
    shmem_cpr_reserve(1, a, array_size, shmem_cpr_pe_num(me));
    /**/
    shmem_barrier_all();

    for ( (*iter)=0; (*iter)<40; ++(*iter) )
    {
        // if ( cpr_pe[me] == 9 || cpr_pe[me] == 10 )
        // {
        //     printf("*** me=%d iter=%d\n", me, *iter);
        //     // for ( j=0; j<array_size; ++j )
        //     //     printf("**%d ", a[j]);
        // }
        if ( (*iter) % 10 == 0)
        {
            shmem_cpr_checkpoint(0, iter, 1, shmem_cpr_pe_num(me));
            shmem_barrier_all();
            // printf("pe=%d done with %lu chp id=0\n", me, *iter);
            
            shmem_cpr_checkpoint(1, a, array_size, shmem_cpr_pe_num(me));
            shmem_barrier_all();
            // printf("pe=%d done with %lu chp id=1\n", me, *iter);

            // for ( i=8; i<11; ++i )
            // {
            //     if ( me == i )
            //     {
            //         printf("PE=%d table:\n", i);
            //         for ( j=0; j<cpr_num_active_pes; ++j )
            //         {
            //             printf("for PE=%d\n", j);
            //             for ( k=0; k<cpr_table_tail[j]; ++k )
            //             {
            //                 printf("pe=%d id=%d count=%d is_symmetric=%d data=:\n",
            //                     cpr_checkpoint_table[j][k][0].pe_num,
            //                     cpr_checkpoint_table[j][k][0].id,
            //                     cpr_checkpoint_table[j][k][0].count,
            //                     cpr_checkpoint_table[j][k][0].is_symmetric);
            //                 for ( l=0; l< cpr_checkpoint_table[j][k][0].count; ++l )
            //                     printf("%d ", cpr_checkpoint_table[j][k][0].data[l]);
            //                 printf("\n----------------\n");
            //             }
            //             printf("============***============\n");
            //         }
            //         printf("\n\n\n");
            //     }
            //     shmem_barrier_all();
            // }
        }
        for ( j=0; j<array_size; ++j)
            a[j] ++;
        
        // if ( (*iter) == 25 && first_rollback == 0 ){
        //     first_rollback = 1;
        //     shmem_cpr_rollback(3, shmem_cpr_pe_num(me));
        //     if ( cpr_pe_role == CPR_STORAGE_ROLE )
        //         *iter = 20;
        //     shmem_barrier_all();
        //     // printf("PE=%d done with rollback with iter=%d!\n", me, *iter);
        //     // if ( me == 11)
        //     // {
        //     //     printf("AFTER ROLLBACK:\n");
        //     //     for ( j=0; j<array_size; ++j )
        //     //         printf("%d ", a[j]);
        //     //     printf("\n");
        //     // }
        // }
    }

    if ( cpr_pe_role == CPR_STORAGE_ROLE )
        shmem_cpr_checkpoint(0, NULL, 0, shmem_cpr_pe_num(me));

    shmem_barrier_all();

    if ( me == 0 )
    {
        for ( i=0; i<npes; ++i )
            printf("pe[%d]=%d, role=%d, type=%d\n", i, cpr_pe[i], cpr_all_pe_role[i], cpr_all_pe_type[i]);
        printf("storages=%d\n", cpr_num_storage_pes);
        for ( i=0; i<cpr_num_storage_pes; ++i )
            printf("storag[%d]=%d \n", i, cpr_storage_pes[i]);
    }

    shmem_barrier_all();
    // if ( cpr_pe_role == CPR_STORAGE_ROLE )
    //     shmem_cpr_checkpoint(1, a, array_size, me);
    // shmem_barrier_all();

    // if (me ==0)
    //     printf("*****\n*****\nAfter Checkpointing\n*****\n*****\n");
    // // // TEST SUCCESSFUL:
    for ( i=8; i<11; ++i )
    {
        if ( me == i )
        {
            printf("PE=%d table:\n", i);
            for ( j=0; j<cpr_num_active_pes; ++j )
            {
                printf("for PE=%d\n", j);
                for ( k=0; k<cpr_table_tail[j]; ++k )
                {
                    printf("pe=%d id=%d count=%d is_symmetric=%d data=:\n",
                        cpr_checkpoint_table[j][k][0]->pe_num,
                        cpr_checkpoint_table[j][k][0]->id,
                        cpr_checkpoint_table[j][k][0]->count,
                        cpr_checkpoint_table[j][k][0]->is_symmetric);
                    for ( l=0; l< cpr_checkpoint_table[j][k][0]->count; ++l )
                        printf("%lu ", cpr_checkpoint_table[j][k][0]->data[l]);
                    printf("\n----------------\n");
                }
                printf("============***============\n");
            }
            printf("\n\n\n");
        }
        shmem_barrier_all();
    }
    shmem_barrier_all ();
    // // I need this part only for testing the whole checkpointing, to make sure nothing's left in queues
    // if ( cpr_pe_type == CPR_SPARE_PE )
    //     shmem_cpr_checkpoint(100, &me, me, me);

    // shmem_barrier_all ();

    // for ( l=8; l<11; ++l ){
        if ( me == 8 )
        {
            cpr_check_carrier *carr;
            
            printf("PE=%d rqh=%d rqt=%d, cqh=%d cqt=%d\n",
                l, cpr_resrv_queue_head, cpr_resrv_queue_tail,
                cpr_check_queue_head, cpr_check_queue_tail);
            for ( i=0; i < cpr_num_active_pes; ++i )
                printf("for PE=%d, we have %d carriers\n", i, cpr_table_tail[i]);
            printf("-------\n");
            for ( i=0; i<cpr_resrv_queue_tail; ++i )
                printf("rcarr[%d].pe_num=%d, id=%d, count=%d, sym=%d\n", 
                    i, cpr_resrv_queue[i].pe_num, cpr_resrv_queue[i].id, 
                    cpr_resrv_queue[i].count, cpr_resrv_queue[i].is_symmetric);
            printf("=======\n");
            for ( i=0; i<cpr_check_queue_tail; ++i )
                printf("chcarr[%d].pe_num=%d, id=%d, count=%d, sym=%d\n", 
                    i, cpr_check_queue[i].pe_num, cpr_check_queue[i].id, 
                    cpr_check_queue[i].count, cpr_check_queue[i].is_symmetric);
            printf("\n\n\n");
        }
        shmem_barrier_all();
    // }

    // // shmem_barrier_all();

    // // for ( j=0; j<npes; ++j )
    // //    if ( me == j )
    // //    {
    // //        printf("I am =%d, called %d reservs and %d checks,\t posted %d reservs and %d checks,\t and read %d reservs and %d checks\n", me, called_resrv, called_check, posted_resrv, posted_check, read_resrv, read_check);
    // //    }
    // /*
    // if ( me == 0)
    //     printf("\nFINALLY\n");
    // printf("PE %d: \t a[0]=%d\n", me, a[0]);
    // */
    shmem_finalize();

    return 0;
}