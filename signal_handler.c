
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/syscall.h>

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))

int handler_count = 0;
void int_handler1(int signal)
{
    printf("sleep in signal handler 1: %d, tid: %d\n", ++handler_count, gettid());
    sleep(3);
    printf("sleep in signal handler 1:%d, tid:%d over...\n", handler_count, gettid());
}

int main()
{
    struct sigaction sa;
    sa.sa_handler = int_handler1;
    // sigfillset(&sa.sa_mask);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags |= SA_RESTART | SA_NODEFER;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    sleep(5);
    printf("sleep 5 seconds over, tid:%d...\n", gettid());
    sigaction(SIGINT, &sa, NULL);
    sleep(6);
    printf("sleep 6 seconds over, tid:%d...\n", gettid());
    return 0;
}
