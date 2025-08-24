//
// Created by awsaf on 7/29/2025.
//

#include "message.h"
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

#define MAX_PROCS 100
#define MAX_CONTEXTS 10000

//Table for sender/reciever pair to synchronize message
typedef struct {
    pthread_mutex_t lock;
    real_priority *sender_waiting;
    real_priority *receiver_waiting;
    int sender_addr;
    int receiver_addr;
} process_comm_table;

static process_comm_table coms_table[MAX_CONTEXTS];

//List for ready processes
static pthread_mutex_t ready_lock = PTHREAD_MUTEX_INITIALIZER;
static real_priority *ready_list[2 * MAX_PROCS];
static int ready_count = 0;

/***
*Create and initialize all tables
*/
void create_message() {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        pthread_mutex_init(&(coms_table[i].lock), NULL);
        coms_table[i].receiver_waiting = NULL;
        coms_table[i].sender_waiting = NULL;
        coms_table[i].sender_addr = 0;
        coms_table[i].receiver_addr = 0;
    }
    ready_count = 0;
}
/***
*This function is responsible for sending message if reciever is waiting
*If the reciever is not ready sender waits
*/
void send_message(real_priority *sender, int receiver_addr) {
    int sender_addr = sender->thread * 100 + sender->id;
    int idx = sender_addr;
    process_comm_table *table = &coms_table[idx];

    pthread_mutex_lock(&(table->lock));

    if (table->receiver_waiting && table->receiver_addr == sender_addr) {
        pthread_mutex_lock(&ready_lock);
        ready_list[ready_count++] = table->receiver_waiting;
        ready_list[ready_count++] = sender;
        pthread_mutex_unlock(&ready_lock);
        table->receiver_waiting = NULL;
        table->receiver_addr = 0;
    }
    else {
        table->sender_waiting = sender;
        table->sender_addr = receiver_addr;
    }
    pthread_mutex_unlock(&(table->lock));
}
/***
*This function is responsible for recieving message from a sender
*If the sender is waiting both are turned into ready
*else the reciver waits till the sender sends message
*/
void receive_message(real_priority *receiver, int sender_addr) {
    int receiver_addr = receiver->thread * 100 + receiver->id;
    int idx = sender_addr;
    process_comm_table *table = &coms_table[idx];

    pthread_mutex_lock(&(table->lock));

    if (table->sender_waiting && table->sender_addr == receiver_addr) {
        pthread_mutex_lock(&ready_lock);
        ready_list[ready_count++] = table->sender_waiting;
        ready_list[ready_count++] = receiver;
        pthread_mutex_unlock(&ready_lock);
        table->sender_waiting = NULL;
        table->sender_addr = 0;
    }
    else {
        table->receiver_waiting = receiver;
        table->receiver_addr = sender_addr;
    }
    pthread_mutex_unlock(&(table->lock));
}
/***
*Returns the processes in an array which has just become ready
*Removes those process from the global ready list
*/
real_priority **message_ready(int *num_ready, int node_id) {
    static real_priority *local_ready[MAX_PROCS];
    int count = 0;

    pthread_mutex_lock(&ready_lock);
    int new_ready_count = 0;
    for (int i = 0; i < ready_count; i++) {
        if (ready_list[i]->thread == node_id) {
            local_ready[count++] = ready_list[i];
        }
        else {
            ready_list[new_ready_count++] = ready_list[i];
        }
    }

    ready_count = new_ready_count;
    pthread_mutex_unlock(&ready_lock);

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (local_ready[i]->id > local_ready[j]->id) {
                real_priority *tmp = local_ready[i];
                local_ready[i] = local_ready[j];
                local_ready[j] = tmp;
            }
        }
    }
    *num_ready = count;
    return local_ready;
}
/***
*Returns true if any process is waiting or else otherwise
*
*/
int message_pending() {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        pthread_mutex_lock(&(coms_table[i].lock));
        if (coms_table[i].sender_waiting != NULL || coms_table[i].receiver_waiting != NULL) {
            pthread_mutex_unlock(&(coms_table[i].lock));
            return 1;
        }
        pthread_mutex_unlock(&(coms_table[i].lock));
    }
    pthread_mutex_lock(&ready_lock);
    int has_global_ready = (ready_count > 0);
    pthread_mutex_unlock(&ready_lock);
    return has_global_ready;
}