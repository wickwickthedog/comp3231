#include "opt-synchprobs.h"
#include <types.h>  /* required by lib.h */
#include <lib.h>    /* for kprintf */
#include <synch.h>  /* for P(), V(), sem_* */
#include <thread.h> /* for thread_fork() */
#include <test.h>

#include "dining_driver.h"

/*
 * Declare any data structures you might need to synchronise 
 * your forks here.
 */
#define LEFT (phil_num + NUM_PHILOSOPHERS - 1) % NUM_PHILOSOPHERS
#define LEFT_LEFT (phil_num - 1 + NUM_PHILOSOPHERS -1) % NUM_PHILOSOPHERS
#define RIGHT (phil_num + 1) % NUM_PHILOSOPHERS
#define RIGHT_RIGHT (phil_num + 2) %NUM_PHILOSOPHERS
#define THINKING 0
#define HUNGRY 1
#define EATING 2
int status[NUM_PHILOSOPHERS];
struct semaphore *mutex;
struct semaphore *s[NUM_PHILOSOPHERS];

/*
 * Take forks ensures mutually exclusive access to two forks
 * associated with the philosopher.
 * 
 * The left fork number = phil_num
 * The right fork number = (phil_num + 1) % NUM_PHILOSPHERS
 */

void take_forks(unsigned long phil_num)
{
    //(void) phil_num;
    P(mutex);
    status[phil_num] = HUNGRY;
    if (status[phil_num] == HUNGRY && status[LEFT] != EATING && status[RIGHT] != EATING) {
    	status[phil_num] = EATING;
    	V(s[phil_num]);
    }
    V(mutex);
    P(s[phil_num]);
}


/*
 * Put forks releases the mutually exclusive access to the
 * philosophers forks.
 */

void put_forks(unsigned long phil_num)
{
    //(void) phil_num;
    P(mutex);
    status[phil_num] = THINKING;
    if (status[LEFT] == HUNGRY && status[LEFT_LEFT] != EATING && status[phil_num] != EATING) {
    	status[LEFT] = EATING;
    	V(s[LEFT]);
    }
    if (status[RIGHT] == HUNGRY && status[phil_num] != EATING && status[RIGHT_RIGHT] != EATING) {
    	status[RIGHT] = EATING;
    	V(s[RIGHT]);
    }

    V(mutex);
}


/* 
 * Create gets called before the philosopher threads get started.
 * Insert any initialisation code you require here.
 */

void create_forks()
{
	mutex = sem_create("mutex", 1);
	KASSERT(mutex != 0);
	for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
		s[i] = sem_create("s[%d]", 0);
		KASSERT(s[i] != 0);

		status[i] = THINKING;
	}
}


/*
 * Destroy gets called when the system is shutting down.
 * You should clean up whatever you allocated in create_forks()
 */

void destroy_forks()
{
    sem_destroy(mutex);
   	for (int i = 0; i < NUM_PHILOSOPHERS; i++) sem_destroy(s[i]);
}
