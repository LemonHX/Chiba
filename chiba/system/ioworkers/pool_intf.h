#pragma once

#include "../../basic_types.h"

typedef struct chiba_iocpu_workerpool *threadpool;

/**
 * @brief  Initialize threadpool
 *
 * Initializes a threadpool. This function will not return until all
 * threads have initialized successfully.
 *
 * @example
 *
 *    ..
 *    threadpool thpool;                     //First we declare a threadpool
 *    thpool = chiba_iocpu_workerpoolinit(4);               //then we initialize
 * it to 4 threads
 *    ..
 *
 * @param  num_threads   number of threads to be created in the threadpool
 * @return threadpool    created threadpool on success,
 *                       NULL on error
 */
threadpool chiba_iocpu_workerpoolinit(i32 num_threads);

/**
 * @brief Add work to the job queue
 *
 * Takes an action and its argument and adds it to the threadpool's job queue.
 * If you want to add to work a function with more than one arguments then
 * a way to implement this is by passing a pointer to a structure.
 *
 * NOTICE: You have to cast both the function and argument to not get warnings.
 *
 * @example
 *
 *    void print_num(i32 num){
 *       printf("%d\n", num);
 *    }
 *
 *    i32 main() {
 *       ..
 *       i32 a = 10;
 *       chiba_iocpu_workerpooladd_work(thpool, (void*)print_num, (void*)a);
 *       ..
 *    }
 *
 * @param  threadpool    threadpool to which the work will be added
 * @param  function_p    pointer to function to add as work
 * @param  arg_p         pointer to an argument
 * @return 0 on success, -1 otherwise.
 */
i32 chiba_iocpu_workerpooladd_work(threadpool, void (*function_p)(void *),
                                   void *arg_p);

/**
 * @brief Wait for all queued jobs to finish
 *
 * Will wait for all jobs - both queued and currently running to finish.
 * Once the queue is empty and all work has completed, the calling thread
 * (probably the main program) will continue.
 *
 * Smart polling is used in wait. The polling is initially 0 - meaning that
 * there is virtually no polling at all. If after 1 seconds the threads
 * haven't finished, the polling interval starts growing exponentially
 * until it reaches max_secs seconds. Then it jumps down to a maximum polling
 * interval assuming that heavy processing is being used in the threadpool.
 *
 * @example
 *
 *    ..
 *    threadpool thpool = chiba_iocpu_workerpoolinit(4);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    chiba_iocpu_workerpoolwait(thpool);
 *    puts("All added work has finished");
 *    ..
 *
 * @param threadpool     the threadpool to wait for
 * @return nothing
 */
void chiba_iocpu_workerpoolwait(threadpool);

/**
 * @brief Pauses all threads immediately
 *
 * The threads will be paused no matter if they are idle or working.
 * The threads return to their previous states once chiba_iocpu_workerpoolresume
 * is called.
 *
 * While the thread is being paused, new work can be added.
 *
 * @example
 *
 *    threadpool thpool = chiba_iocpu_workerpoolinit(4);
 *    chiba_iocpu_workerpoolpause(thpool);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    chiba_iocpu_workerpoolresume(thpool); // Let the threads start their magic
 *
 * @param threadpool    the threadpool where the threads should be paused
 * @return nothing
 */
void chiba_iocpu_workerpoolpause(threadpool);

/**
 * @brief Unpauses all threads if they are paused
 *
 * @example
 *    ..
 *    chiba_iocpu_workerpoolpause(thpool);
 *    sleep(10);              // Delay execution 10 seconds
 *    chiba_iocpu_workerpoolresume(thpool);
 *    ..
 *
 * @param threadpool     the threadpool where the threads should be unpaused
 * @return nothing
 */
void chiba_iocpu_workerpoolresume(threadpool);

/**
 * @brief Destroy the threadpool
 *
 * This will wait for the currently active threads to finish and then 'kill'
 * the whole threadpool to free up memory.
 *
 * @example
 * i32 main() {
 *    threadpool thpool1 = chiba_iocpu_workerpoolinit(2);
 *    threadpool thpool2 = chiba_iocpu_workerpoolinit(2);
 *    ..
 *    chiba_iocpu_workerpooldestroy(thpool1);
 *    ..
 *    return 0;
 * }
 *
 * @param threadpool     the threadpool to destroy
 * @return nothing
 */
void chiba_iocpu_workerpooldestroy(threadpool);

/**
 * @brief Show currently working threads
 *
 * Working threads are the threads that are performing work (not idle).
 *
 * @example
 * i32 main() {
 *    threadpool thpool1 = chiba_iocpu_workerpoolinit(2);
 *    threadpool thpool2 = chiba_iocpu_workerpoolinit(2);
 *    ..
 *    printf("Working threads: %d\n",
 * chiba_iocpu_workerpoolnum_threads_working(thpool1));
 *    ..
 *    return 0;
 * }
 *
 * @param threadpool     the threadpool of interest
 * @return integer       number of threads working
 */
i32 chiba_iocpu_workerpoolnum_threads_working(threadpool);
