#include "fifoInt.h"

IntQueue * fifoInt_create(int size) {

  IntQueue *q;
  q = (IntQueue *) malloc(sizeof(IntQueue));

  q->max = size;
  q->size = 0;
  q->vector = (int *) malloc(sizeof(int)* (size + 1));
  q->head = 0;
  q->tail = -1;
  return q;
    
}

int fifoInt_empty(IntQueue *q) {
  return (q->size == 0);
}

fifoInt_enqueue(IntQueue *q, int value) {
  if (q->size >= q->max) {
    printf("queue overflow\n");
    return;
  }
  q->size++;
  q->tail = (q->tail +1) % q->max;
  q->vector[q->tail] = value;
}


int fifoInt_dequeue(IntQueue *q) {
  int v;
  if (q->size <= 0) { printf("queue underflow\n"); return; }
  q->size--;
  v = q->vector[q->head];
  q->head = (q->head+1) % q->max;
  return v;
}
