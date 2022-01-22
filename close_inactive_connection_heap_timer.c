#include "heap_timer.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#define FD_LIMIT         65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT         2

static int pipefd[2];

static time_heap th(10);
static int epollfd = 0;
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
/*定时器回调函数, 它删除非活动连接socket上的注册事件, 并关闭之*/
void cb(client_data *user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd:%d\n", user_data->sockfd);
}
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage:%s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    /* start timer */
    th.spawn();
    /*设置信号处理函数*/
    addsig(SIGTERM);
    bool stop_server = false;
    struct client_data *users = new struct client_data[FD_LIMIT];
    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            /*处理新到的客户连接*/
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                addfd(epollfd, connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                /*创建定时器, 设置其回调函数与超时时间, 然后绑定定时器与用户数据,
                 * 最后将定时器添加到链time_heap中*/
                heap_timer *timer = new heap_timer(3 * TIMESLOT);
                timer->user_data = &users[connfd];
                timer->cb = cb;
                users[connfd].timer = timer;
                th.add_timer(timer);
            }
            /*处理信号*/
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    // handle the error
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGTERM: {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            /*处理客户连接上接收到的数据*/
            else if (events[i].events & EPOLLIN)
            {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data: %s from %d\n", ret, users[sockfd].buf, sockfd);
                heap_timer *timer = users[sockfd].timer;
                if (ret < 0)
                {
                    /*如果发生读错误, 则关闭连接, 并移除其对应的定时器*/
                    if (errno != EAGAIN)
                    {
                        cb(&users[sockfd]);
                        if (timer)
                        {
                            th.del_timer(timer);
                        }
                    }
                }
                else if (ret == 0)
                {
                    /*如果对方已经关闭连接, 则我们也关闭连接, 并移除对应的定时器*/
                    cb(&users[sockfd]);
                    if (timer)
                    {
                        th.del_timer(timer);
                    }
                }
                else
                {
                    /*如果某个客户连接上有数据可读, 则我们要调整该连接对应的定时器,
                     * 以延迟该连接被关闭的时间*/
                    if (timer)
                    {
                        printf("adjust timer once\n");
                        heap_timer *new_timer = new heap_timer(3 * TIMESLOT);
                        new_timer->user_data = timer->user_data;
                        new_timer->cb = timer->cb;
                        new_timer->user_data->timer = new_timer;
                        th.del_timer(timer);
                        th.add_timer(new_timer);
                    }
                }
            }
            else
            {
                // others
            }
        }
    }
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    return 0;
}