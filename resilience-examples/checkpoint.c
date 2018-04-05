#include <stdio.h>

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


void fenix_set_pe_type (int me, int npes, int spes)
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


int fenix_init(int me, int npes, int spes)
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
    fenix_set_pe_type (me, npes, spes);

    switch (pe_type)
    {
        case ORIGINAL_PE:
            shadow_mem_size = 1;
            shadow_mem = (void **) malloc(shadow_mem_size * sizeof(void *) );
            break;

        case SPARE_PE:
            // can we add something here that prevents spares
            // from running the code between fenix_init and finalize as others?
            break;

        case MSPE:
            checkpoint_table = (void ***) malloc(num_opes * sizeof(void **));
            for (int i=0; i<num_opes; ++i)
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

// for now, assuming we are checkpointing ints
void fenix_checkpoint ( void* mem, int size, int pe_num )
{
    /* TO DO:
    add the name and type of the variables (to be used in spare PEs)
    */
    switch (pe_type)
    {
        case ORIGINAL_PE:
        case RESURRECTED_PE:
            shadow_mem_size ++;
            break;

        case SPARE_PE:
            break;

        case MSPE:
            break;

        default:
        // nothing here for now
            break;
    }
}

int
main ()
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

    int res = fenix_init(me,npes, spes);
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
            fenix_checkpoint(&i, 1, me);
            fenix_checkpoint(a, 10, me);
        }
        for ( j=0; j<10; ++j)
            a[j] ++;
        if ( i == 25 ){
            fenix_rollback();
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