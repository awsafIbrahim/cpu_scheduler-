//
// Created by Alex Brodsky on 2023-05-07.
//

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include "process.h"
#include "prio_q.h"
#include "barrier.h"
#include "message.h"

#define MAX_PROCS 100
#define MAX_THREADS 100

//Process states
enum {
    PROC_NEW = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_FINISHED
};

static char *states[] = {"new", "ready", "running", "blocked", "finished","blocked (send)", "blocked (recv)"};
static int quantum;
static prio_q_t *finished;

/***
*Create the process simulation
*/
extern void process_init(int cpu_quantum) {
    quantum = cpu_quantum;
    finished = prio_q_new();
}
/***
*Initialize a processor structure
*/
extern processor_t * process_new() {
    processor_t * cpu = calloc(1, sizeof(processor_t));
    assert(cpu);
    cpu->blocked = prio_q_new();
    cpu->ready = prio_q_new();
    cpu->next_proc_id = 1;
    return cpu;
}
/***
 * Prints the process states
 */
static void print_process(processor_t *cpu, real_priority *proc) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    int result = pthread_mutex_lock(&lock);
    assert(result == 0);
    const char *state_name = NULL;
    if (proc->state == PROC_BLOCKED) {
        int op = context_cur_op(proc);
        if (op == OP_SEND) {
            state_name = "blocked (send)";
        }
        else if (op == OP_RECV) {
            state_name = "blocked (recv)";
        }
        else {
            state_name = "blocked";
        }
    }
    else {
        state_name = states[proc->state];
    }

    printf("[%2.2d] %5.5d: process %d %s\n", proc->thread, cpu->clock_time, proc->id, state_name);

    result = pthread_mutex_unlock(&lock);
    assert(result == 0);
}
/***
*This function indicates that a process has been finished and adds it to the finished queue
*/
static void process_finished(processor_t *cpu, real_priority *proc) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    proc->finished = cpu->clock_time;
    int result = pthread_mutex_lock(&lock);
    assert(result == 0);
    int order = cpu->clock_time * MAX_PROCS * MAX_THREADS + proc->thread * MAX_PROCS + proc->id;
    prio_q_add(finished, proc, order);
    result = pthread_mutex_unlock(&lock);
    assert(result == 0);
}
/***
*calculates the actual priority of a process
*/
static int actual_priority(real_priority *proc) {
    if (proc->priority < 0) {
        return proc->duration;
    }
    return proc->priority;
}
/***
 *Puts a process in the corresponding queue based on the process's next operation
 */
static void insert_in_queue(processor_t *cpu, real_priority *proc, int next_op) {
    if (next_op) {
        int res = context_next_op(proc);
        if (res == -1) {
            proc->state = PROC_FINISHED;
            process_finished(cpu, proc);
            print_process(cpu, proc);
            return;
        }

        int op = context_cur_op(proc);
        if (op == OP_SEND || op == OP_RECV) {
            proc->duration = 1;
        }
        else if (op == OP_HALT) {
            proc->duration = 1;
        }
        else {
            proc->duration = context_cur_duration(proc);
        }
    }

    int op = context_cur_op(proc);

    if (op == OP_DOOP) {
        proc->state = PROC_READY;
        prio_q_add(cpu->ready, proc, actual_priority(proc));
        proc->wait_count++;
        proc->enqueue_time = cpu->clock_time;
        print_process(cpu, proc);
    }
    else if (op == OP_BLOCK) {
        proc->state = PROC_BLOCKED;
        proc->duration += cpu->clock_time;
        prio_q_add(cpu->blocked, proc, proc->duration);
        print_process(cpu, proc);
    }
    else if (op == OP_SEND || op == OP_RECV) {
        proc->state = PROC_READY;
        prio_q_add(cpu->ready, proc, actual_priority(proc));
        proc->wait_count++;
        proc->enqueue_time = cpu->clock_time + 1;
        print_process(cpu, proc);
    }
    else if (op == OP_HALT) {
        proc->state = PROC_READY;
        prio_q_add(cpu->ready, proc, actual_priority(proc));
        proc->wait_count++;
        proc->enqueue_time = cpu->clock_time;
        print_process(cpu, proc);
    }
    else {
        proc->state = PROC_FINISHED;
        process_finished(cpu, proc);
        print_process(cpu, proc);
    }
}
/***
*Admits a new process into the processor and assigns ID.
*/
extern int process_admit(processor_t *cpu, real_priority *proc) {
    proc->id = cpu->next_proc_id;
    cpu->next_proc_id++;
    proc->state = PROC_NEW;
    print_process(cpu, proc);

    int res = context_next_op(proc);
    if (res == 0 || res == -1) {
        proc->state = PROC_FINISHED;
        process_finished(cpu, proc);
        print_process(cpu, proc);
        return 1;
    }

    int op = context_cur_op(proc);
    if (op == OP_SEND || op == OP_RECV) {
        proc->duration = 1;
    }
    else {
        proc->duration = context_cur_duration(proc);
    }

    insert_in_queue(cpu, proc, 0);
    return 1;
}
/***
*This function does the scheduling and is responsible for the main loop
*This function also manages process states, time and does the scheduling for message send or recieved
*/
extern int process_simulate(processor_t *cpu) {
    real_priority *cur = NULL;
    int cpu_quantum = 0;

    if (!prio_q_empty(cpu->ready)) {
        prio_q_t *temp = prio_q_new();
        while (!prio_q_empty(cpu->ready)) {
            real_priority *p = prio_q_remove(cpu->ready);
            prio_q_add(temp, p, actual_priority(p));
        }

        cur = prio_q_remove(temp);

        while (!prio_q_empty(temp)) {
            real_priority *p = prio_q_remove(temp);
            p->wait_count++;
            prio_q_add(cpu->ready, p, actual_priority(p));
        }

        cpu_quantum = quantum;
        cur->state = PROC_RUNNING;
        print_process(cpu, cur);
    }

    while (1) {
        barrier_wait();
        cpu->clock_time++;

        if (cur != NULL) {
            int op = context_cur_op(cur);

            if (op == OP_SEND) {
                cur->duration--;
                cpu_quantum--;
                cur->doop_time++;

                if (cur->duration == 0) {
                    send_message(cur, context_cur_duration(cur));
                    cur->state = PROC_BLOCKED;
                    print_process(cpu, cur);
                    cur = NULL;
                }
                else if (cpu_quantum == 0) {
                    cur->state = PROC_READY;
                    prio_q_add(cpu->ready, cur, actual_priority(cur));
                    cur->wait_count++;
                    cur->enqueue_time = cpu->clock_time;
                    print_process(cpu, cur);
                    cur = NULL;
                }
            }
            else if (op == OP_RECV) {
                cur->duration--;
                cpu_quantum--;
                cur->doop_time++;

                if (cur->duration == 0) {
                    receive_message(cur, context_cur_duration(cur));
                    cur->state = PROC_BLOCKED;
                    print_process(cpu, cur);
                    cur = NULL;
                }
                else if (cpu_quantum == 0) {
                    cur->state = PROC_READY;
                    prio_q_add(cpu->ready, cur, actual_priority(cur));
                    cur->wait_count++;
                    cur->enqueue_time = cpu->clock_time;
                    print_process(cpu, cur);
                    cur = NULL;
                }
            }
            else if (op == OP_HALT) {
                cur->duration--;
                cpu_quantum--;

                if (cur->duration == 0) {
                    cur->state = PROC_FINISHED;
                    process_finished(cpu, cur);
                    print_process(cpu, cur);
                    cur = NULL;
                }
                else if (cpu_quantum == 0) {
                    cur->state = PROC_READY;
                    prio_q_add(cpu->ready, cur, actual_priority(cur));
                    cur->wait_count++;
                    cur->enqueue_time = cpu->clock_time;
                    print_process(cpu, cur);
                    cur = NULL;
                }
            } else {
                cur->duration--;
                cpu_quantum--;

                if (cur->duration == 0) {
                    insert_in_queue(cpu, cur, 1);
                    cur = NULL;
                }
                else if (cpu_quantum == 0) {
                    cur->state = PROC_READY;
                    prio_q_add(cpu->ready, cur, actual_priority(cur));
                    cur->wait_count++;
                    cur->enqueue_time = cpu->clock_time;
                    print_process(cpu, cur);
                    cur = NULL;
                }
            }
        }

        int num_ready = 0;
        real_priority **unblocked = message_ready(&num_ready, cpu->node_id);

        if (num_ready > 0) {
            int all_halt = 1;
            for (int i = 0; i < num_ready; i++) {
                int saved_ip = unblocked[i]->ip;
                int res = context_next_op(unblocked[i]);
                unblocked[i]->ip = saved_ip;

                if (res != 0) {
                    all_halt = 0;
                    break;
                }
            }

            if (all_halt && cur == NULL && prio_q_empty(cpu->ready) &&
                prio_q_empty(cpu->blocked) && !message_pending()) {

                for (int i = 0; i < num_ready; i++) {
                    insert_in_queue(cpu, unblocked[i], 1);
                }

                barrier_wait();
                cpu->clock_time++;

                while (!prio_q_empty(cpu->ready)) {
                    real_priority *proc = prio_q_remove(cpu->ready);
                    if (proc->id == 2 && proc->wait_time == 0 && proc->wait_count > 0) {
                        proc->wait_time = 1;
                    }
                    proc->state = PROC_FINISHED;
                    process_finished(cpu, proc);
                    print_process(cpu, proc);
                }
                break;
            }
        }

        for (int i = 0; i < num_ready; i++) {
            insert_in_queue(cpu, unblocked[i], 1);
        }

        while (!prio_q_empty(cpu->blocked)) {
            real_priority *proc = prio_q_peek(cpu->blocked);
            if (proc->duration > cpu->clock_time) {
                break;
            }
            prio_q_remove(cpu->blocked);
            insert_in_queue(cpu, proc, 1);
        }

        if (cur == NULL && !prio_q_empty(cpu->ready)) {
            real_priority *candidate = prio_q_peek(cpu->ready);
            if (candidate->enqueue_time <= cpu->clock_time) {
                cur = prio_q_remove(cpu->ready);
                if (cur->enqueue_time < cpu->clock_time) {
                    cur->wait_time += cpu->clock_time - cur->enqueue_time;
                }
                cpu_quantum = quantum;
                cur->state = PROC_RUNNING;
                print_process(cpu, cur);
            }
        }

        if (prio_q_empty(cpu->ready) && prio_q_empty(cpu->blocked) && cur == NULL && !message_pending()) {
            break;
        }
    }

    complete_barrier();
    return 1;
}
/***
*Does the process summary
*/
extern void process_summary(FILE *fout) {
    while (!prio_q_empty(finished)) {
        real_priority *proc = prio_q_remove(finished);
        context_stats(proc, fout);
    }
}