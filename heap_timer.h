#ifndef MIN_HEAP
#define MIN_HEAP
#include <iostream>
#include <netinet/in.h>
#include <thread>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "criticsec.h"
using std::exception;
#define BUFFER_SIZE 64
class heap_timer; /*前向声明*/
/*绑定socket和定时器*/
struct client_data {
  sockaddr_in address;
  int sockfd;
  char buf[BUFFER_SIZE];
  heap_timer *timer;
};
typedef void (*cb_func)(client_data *); /*定时器的回调函数*/
/*定时器类*/
class heap_timer {
public:
  heap_timer(int delay) { expire = time(NULL) + delay; }

public:
  int slot; /* 定时器在数组中的索引 */
  time_t expire;                  /*定时器生效的绝对时间*/
  cb_func cb; /*定时器的回调函数*/
  client_data *user_data;         /*用户数据*/
};
/*时间堆类*/
class time_heap {
public:
  /*构造函数之一, 初始化一个大小为cap的空堆*/
  time_heap(int cap) throw(std::exception) : capacity(cap), cur_size(0) {
    Sem_init(&mutex, 0, 1);
    Event_init(&event);
    array = new heap_timer *[capacity]; /*创建堆数组*/
    if (!array) {
      throw std::exception();
    }
    for (int i = 0; i < capacity; ++i) {
      array[i] = NULL;
    }
  }
  /*构造函数之二, 用已有数组来初始化堆*/
  time_heap(heap_timer **init_array, int size,
            int capacity) throw(std::exception)
      : cur_size(size), capacity(capacity) {
    if (capacity < size) {
      throw std::exception();
    }
    Sem_init(&mutex, 0, 1);
    Event_init(&event);
    array = new heap_timer *[capacity]; /*创建堆数组*/
    if (!array) {
      throw std::exception();
    }
    for (int i = 0; i < capacity; ++i) {
      array[i] = NULL;
    }
    if (size != 0) {
      /*初始化堆数组*/
      for (int i = 0; i < size; ++i) {
        array[i] = init_array[i];
        array[i]->slot = i;
      }
      for (int i = (cur_size - 1) / 2; i >= 0;
           --i) { /*对数组中的第[(cur_size-1)/2]～0个元素执行下虑操作*/
        percolate_down(i);
      }
    }
  }
  /*销毁时间堆*/
  ~time_heap() {
    for (int i = 0; i < cur_size; ++i) {
      delete array[i];
    }
    delete[] array;
    sem_destroy(&mutex);
    Event_destroy(&event);
  }

public:
  /*添加目标定时器timer*/
  void add_timer(heap_timer *timer) throw(std::exception) {
    if (!timer) {
      return;
    }
    P(&mutex);
    if (cur_size >= capacity) /*如果当前堆数组容量不够, 则将其扩大1倍*/
    {
      resize();
    }
    /*新插入了一个元素, 当前堆大小加1, hole是新建空穴的位置*/
    int hole = cur_size++;
    int parent = 0;
    /*对从空穴到根节点的路径上的所有节点执行上虑操作*/
    for (; hole > 0; hole = parent) {
      parent = (hole - 1) / 2;
      if (array[parent]->expire <= timer->expire) {
        break;
      }
      array[hole] = array[parent];
      array[hole]->slot = hole;
    }
    array[hole] = timer;
    array[hole]->slot = hole;
    printf("add new timer(fd: %d) to heap, in slot: %d, current count: %d\n", timer->user_data->sockfd, hole, cur_size);
    V(&mutex);
    Event_set(&event);
  }
  /*删除目标定时器timer*/
  void del_timer(heap_timer *timer) {
    if (!timer) {
      return;
    }
    /*仅仅将目标定时器的回调函数设置为空,
     * 即所谓的延迟销毁。这将节省真正删除该定时器造成的开销,
     * 但这样做容易使堆数组膨胀*/
    timer->cb = NULL;
    /*删除此节点*/
    P(&mutex);
    int slot = timer->slot;
    array[slot] = array[--cur_size];
    percolate_down(slot);
    printf("delete timer(fd: %d) from heap, slot: %d, current count: %d\n", timer->user_data->sockfd, slot, cur_size);
    delete timer;

    V(&mutex);

    Event_set(&event);
  }
  /*获得堆顶部的定时器*/
  heap_timer *top() const {
    if (empty()) {
      return NULL;
    }
    return array[0];
  }
  /*删除堆顶部的定时器*/
  void pop_timer() {
    if (empty()) {
      return;
    }
    P(&mutex);
    if (array[0]) {
      printf("pop timer in slot 0, fd:%d\n", array[0]->user_data->sockfd);
      delete array[0];
      /*将原来的堆顶元素替换为堆数组中最后一个元素*/
      array[0] = array[--cur_size];
      percolate_down(0); /*对新的堆顶元素执行下虑操作*/
    }
    V(&mutex);
    Event_set(&event);
  }
  /*心搏函数*/
  void tick() {
    while(1)
    {
      int sleepTime = INFINITE;
      P(&mutex);
      heap_timer *tmp = array[0];
      time_t cur = time(NULL); /*循环处理堆中到期的定时器*/
      while (!empty()) {
        if (!tmp) {
          break;
        }
        printf("ticking....cur timer: %d\n", cur_size);
        /*如果堆顶定时器没到期, 则退出循环*/
        if (tmp->expire > cur) {
          sleepTime = tmp->expire - cur;
          printf("expire: %d, cur: %d, sleep %d seconds\n", tmp->expire, cur, sleepTime);
          break;
        }
        V(&mutex);
        cb_func cb = array[0]->cb;
        /*否则就执行堆顶定时器中的任务*/
        if (cb) {
          cb(array[0]->user_data);
        }
        /*将堆顶元素删除, 同时生成新的堆顶定时器(array[0])*/
        pop_timer();
        tmp = array[0];
        P(&mutex);
        cur = time(NULL);
      }
      V(&mutex);
      Event_wait(&event, sleepTime);
    }


  }
  bool empty() const { return cur_size == 0; }

  void spawn() {
    std::thread t([this] { tick(); });
    t.detach();
  }

private:
  /*最小堆的下虑操作, 它确保堆数组中以第hole个节点作为根的子树拥有最小堆性质*/
  void percolate_down(int hole) {
    if(!cur_size) return;
    heap_timer *temp = array[hole];
    int child = 0;
    for (; ((hole * 2 + 1) <= (cur_size - 1)); hole = child) {
      child = hole * 2 + 1;
      if ((child < (cur_size - 1)) &&
          (array[child + 1]->expire < array[child]->expire)) {
        ++child;
      }
      if (array[child]->expire < temp->expire) {
        array[hole] = array[child];
        array[hole]->slot = hole;
        //printf("------down: put fd:%d to slot: %d", array[hole]->user_data->sockfd, hole);
      } else {
        break;
      }
    }
    array[hole] = temp;
    temp->slot = hole;
    //printf("------down: put fd:%d to slot: %d", array[hole]->user_data->sockfd, hole);
  }
  /*将堆数组容量扩大1倍*/
  void resize() throw(std::exception) {
    heap_timer **temp = new heap_timer *[2 * capacity];
    for (int i = 0; i < 2 * capacity; ++i) {
      temp[i] = NULL;
    }
    if (!temp) {
      throw std::exception();
    }
    capacity = 2 * capacity;
    for (int i = 0; i < cur_size; ++i) {
      temp[i] = array[i];
    }
    delete[] array;
    array = temp;
  }

private:
  heap_timer **array; /*堆数组*/
  int capacity;       /*堆数组的容量*/
  int cur_size;       /*堆数组当前包含元素的个数*/
  sem_t mutex; /*保护时间堆*/
  Event event; /* 用来做睡眠时间配置*/
};
#endif