/**
 * @file msg_queue.h
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

typedef struct MsgNode {
    char msg[256];
    int size;
    int dest;
    struct MsgNode* next;
} MsgNode;

typedef struct MsgQueue {
    int size;
    struct MsgNode* head;
    struct MsgNode* tail;
} MsgQueue;


void MsgQueue_init(MsgQueue* queue);

MsgNode* MsgQueue_front(MsgQueue* queue);

void MsgQueue_append(MsgQueue* queue, char* msg, int size, int dest);

void MsgQueue_pop(MsgQueue* queue);

void MsgQueue_clear(MsgQueue* queue);

void MsgQueue_print(MsgQueue* queue);

#endif//MSG_QUEUE_H