#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "context.h"
#include "process.h"
#include "message.h"
#include "barrier.h"

static real_priority **procs;

typedef struct thread_args {
    int id;         /* Node id of thread */
} thread_args;

/* Node runner
 * @params:
 *   arg : node id of thread
 * @returns:
 *   NULL
 */
static void *thread_runner(void *arg) {
    thread_args *thd_arg = (thread_args *)arg;

    processor_t *cpu = process_new();
    cpu->node_id = thd_arg->id;
    for (int i = 0; procs[i]; i++) {
        if (procs[i]->thread == thd_arg->id) {
            process_admit(cpu, procs[i]);
        }
    }

    // Waiting  for all threads to finish initialization
    barrier_wait();

    process_simulate(cpu);

    return NULL;
}

/* Main line
 * @params:
 *   None
 * @returns:
 *   0
 */
int main() {
    int num_procs;
    int quantum;
    int num_threads;
    thread_args *args;

    /* Read in the header of the process description with minimal validation
     */
    if (scanf("%d %d %d", &num_procs, &quantum, &num_threads) < 3) {
        fprintf(stderr, "Bad input, expecting # of processes, quantum, and # of threads\n");
        return -1;
    }

    /* We use an array of pointers to contexts to track the processes.
     * We also use an array of args for the nodes and an array for thread IDs
     */
    procs  = calloc(num_procs + 1, sizeof(real_priority *));
    args  = calloc(num_procs + 1, sizeof(thread_args));
    pthread_t *tid = calloc(num_threads, sizeof(pthread_t));

    process_init(quantum);
    create_message();
    create_barrier(num_threads);

    /* Load process, if  error occurs, abort.
     */
    for (int i = 0; i < num_procs; i++) {
        procs[i] = context_load(stdin);
        if (!procs[i]) {
            fprintf(stderr, "Bad input, could not load program description\n");
            return -1;
        }
    }


    /* Create threads and assume creation will be successful (or just die)
     */
    for (int i = 0; i < num_threads; i++) {
        /* This is where we assign node ids
         */
        args[i].id = i + 1;
        int result = pthread_create(&tid[i], NULL, thread_runner, &args[i]);
        assert(result == 0);
    }

    /* Wait for threads to complete and assume we will be successful (or just die)
     */
    for (int i = 0; i < num_threads; i++) {
        int result = pthread_join(tid[i], NULL);
        assert(result == 0);
    }


    /* Output the statistics for processes.
     */
    process_summary(stdout);

    return 0;
}