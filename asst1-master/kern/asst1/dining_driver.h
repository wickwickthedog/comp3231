#ifndef DINING_DRIVER_H
#define DINING_DRIVER_H

/* The number of philosphers
 * This will be changed during testing
 */
#define NUM_PHILOSOPHERS 5
#define TIMES_TO_EAT 1000 /* gluttens..... */

extern void take_forks(unsigned long phil_num);
extern void put_forks(unsigned long phil_num);
extern void create_forks(void);
extern void destroy_forks(void);

#endif