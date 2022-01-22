#ifndef CRITICSEC_H
#define CRITICSEC_H
#include <pthread.h>
#include <semaphore.h>
#define INFINITE 0xFFFFFFFF
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Event, *PEvent;

/* POSIX semaphore wrappers */
void Sem_init(sem_t *sem, int pshared, unsigned int value);
void P(sem_t *sem);
void V(sem_t *sem);

void Event_init(PEvent event);
void Event_wait(PEvent event, int timeout);
void Event_set(PEvent event);
void Event_destroy(PEvent event);
#endif