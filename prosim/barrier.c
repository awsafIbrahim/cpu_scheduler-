#include <pthread.h>
#include <assert.h>
#include "barrier.h"
//Global variables

//Mutex to ensure exlusice access
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static int max_threads = 0;
static int waiters = 0;
static int generation = 0;

/**
 *Create the barrier for use with threads
 *@param n indicates the number of threads
 *
 */
void create_barrier(int n) {
    pthread_mutex_lock(&lock);
    max_threads = n;
    waiters = 0;
    generation = 0;
    pthread_mutex_unlock(&lock);
}

/**
 * Wait untill all threads reach this area and after that release
 */
void barrier_wait() {
    pthread_mutex_lock(&lock);
    int gen = generation;

    waiters++;
    if (waiters == max_threads) {
        generation++;
        waiters = 0;
        pthread_cond_broadcast(&cond);
    } else {
        while (gen == generation) {
            pthread_cond_wait(&cond, &lock);
        }
    }

    pthread_mutex_unlock(&lock);
}
/***
*This function indicates that a thread has finished using the barrier
*Decrease the total thread count for future use cases
*/
void complete_barrier() {
    pthread_mutex_lock(&lock);
    max_threads--;
    if (waiters == max_threads) {
        generation++;
        waiters = 0;
        pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&lock);
}
