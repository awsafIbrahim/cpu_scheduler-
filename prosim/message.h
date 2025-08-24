//
// Created by awsaf on 7/29/2025.
//

#ifndef MESSAGE_H
#define MESSAGE_H
#include "context.h"

void create_message();
void send_message(real_priority *sender, int receiver_addr);
void receive_message(real_priority *receiver, int sender_addr);
real_priority **message_ready(int *num_ready,int node_id);
int message_pending();
static int pair_index(int sender_addr, int receiver_addr);
#endif
