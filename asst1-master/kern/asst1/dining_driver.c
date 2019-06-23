#include "opt-synchprobs.h"
#include <types.h>  /* required by lib.h */
#include <lib.h>    /* for kprintf */
#include <synch.h>  /* for P(), V(), sem_* */
#include <thread.h> /* for thread_fork() */
#include <test.h>

#include "dining_driver.h"

static struct semaphore *philosopher_finished;
static unsigned int fork_count[NUM_PHILOSOPHERS];


static void eat(unsigned long philosopher)
{
    fork_count[philosopher]++; /* The 'left' fork */
    fork_count[(philosopher + 1) % NUM_PHILOSOPHERS]++; /* The 'right' fork */
}

static void think(unsigned long philosopher)
{
    (void) philosopher;
}

static void
philosopher_thread(void *unused_ptr, unsigned long thread_num)
{
        int course; /* A many course meal */
        

        (void)unused_ptr; /* Avoid compiler warnings */

        kprintf("Philosopher %ld started\n", thread_num);

        /* now loop for TIMES_TO_EAT course, thinking in-between. */

        for (course = 0; course < TIMES_TO_EAT; course++) {

            think(thread_num);

            take_forks(thread_num); /* Ensure mutually exclusive access 
                                    to left and right forks */ 
            eat(thread_num);   
                
            put_forks(thread_num);  /* Release the left and right forks */
              
        }

        /* Life has come to an end... Signal that we're done. */
        kprintf("Philosopher %ld finished\n", thread_num);
        V(philosopher_finished);
}

int run_philosophers(int data1, char **data2)
{
        int index, error;
        

        /*
         * Avoid unused variable warnings from the compiler.
         */
        (void) data1;
        (void) data2;

        /* create a semaphore to allow main thread to wait on philosophers */

        philosopher_finished = sem_create("finished", 0);

        if (philosopher_finished == NULL) {
                panic("phil finished: sem create failed");
        }


        /* initialise the fork usage count */
        for (index = 0; index < NUM_PHILOSOPHERS; index++){
            fork_count[index] = 0;
        }

        /* Initialise your fork concurrency control */ 
        create_forks();

        /*
         * Start  NUM_PHILOSOPHERS philosopher() threads.
         */

        kprintf("Starting %d philosopher threads\n",  NUM_PHILOSOPHERS);

        for (index = 0; index <  NUM_PHILOSOPHERS; index++) {

                error = thread_fork("philosopher thread", NULL, &philosopher_thread, NULL, index);

                /*
                 * panic() on error as we can't progress if we can't create threads.
                 */

                if (error) {
                        panic("run philosophers: thread_fork failed: %s\n",
                              strerror(error));
                }
        }


        /* 
         * Wait until the philosopher threads complete by waiting on 
         * the semaphore NUM_PHILOSOPHER times.
         */

        for (index = 0; index <  NUM_PHILOSOPHERS; index++) {
                P(philosopher_finished);
        }

        /* Print out some statistics */
        
        for (index = 0; index < NUM_PHILOSOPHERS; index++) {
                kprintf("Fork %d used %d times.\n", index,
                    fork_count[index]);
        }
        
        /* clean up the semaphore we allocated earlier */
        sem_destroy(philosopher_finished);

        /* now clean up your fork concurrency control */
        destroy_forks();
        return 0;
}





