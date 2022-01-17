/*******************************
 * Wrappers for Posix semaphores
 *******************************/
#include"criticsec.h"
#include<cstdio>
#include <sys/time.h>

void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
	printf("Sem_init error\n");
}

void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
	printf("P error\n");
}

void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
	printf("V error\n");
}
void Event_init(PEvent event)
{
    if(pthread_mutex_init(&event->mutex, NULL) < 0)
        printf("event mutex init failed");
    if(pthread_cond_init(&event->cond, NULL) < 0)
        printf("event cond init failed\n");
}
void Event_wait(PEvent event, int timeout)
{
    pthread_mutex_lock(&event->mutex);
    if(timeout == INFINITE || timeout < 0)
    {
        if(pthread_cond_wait(&event->cond, &event->mutex) < 0)
        printf("wait event failed\n");
    }
    else if(timeout > 0)
    {
        struct timespec ts;
        struct timeval tv;

        gettimeofday(&tv, NULL);

        ts.tv_sec = tv.tv_sec + timeout;
        ts.tv_nsec = tv.tv_usec * 1000;
        if(pthread_cond_timedwait(&event->cond, &event->mutex, &ts) < 0)
        printf("wait time event failed\n");
    }

    pthread_mutex_unlock(&event->mutex);
}
void Event_set(PEvent event)
{
    pthread_mutex_lock(&event->mutex);
    if(pthread_cond_signal(&event->cond) < 0)
    printf("signal event failed\n");
    pthread_mutex_unlock(&event->mutex);
}
void Event_destroy(PEvent event)
{
    if(pthread_cond_destroy(&event->cond) < 0)
    printf("destroy event failed\n");
    if(pthread_mutex_destroy(&event->mutex) < 0)
    printf("destroy mutex failed\n");
}