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


void MsgQueue_Init(MsgQueue* queue);

MsgNode* MsgQueue_Front(MsgQueue* queue);

void MsgQueue_Append(MsgQueue* queue, char* msg, int size, int dest);

void MsgQueue_Pop(MsgQueue* queue);

void MsgQueue_Clear(MsgQueue* queue);

void MsgQueue_Print(MsgQueue* queue);

#endif//MSG_QUEUE_H