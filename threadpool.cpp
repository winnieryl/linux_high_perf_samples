#include <iostream>
#include <deque>
#include <future>
#include <thread>
#include <functional>
#include <vector>
#include <condition_variable>
#include "threadpool.hpp"

void ThreadPool::Start()
{
    for (int i = 0; i < _threadSize; ++i)
    {
        threads_.emplace_back(new std::thread(std::bind(&ThreadPool::Run, this)));
    }
    _isRunning = true;
}

Task ThreadPool::Take()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while (tasks_.empty() && _isRunning) _notEmpty.wait(lock);
    Task task;
    if (!tasks_.empty())
    {
        task = std::move(tasks_.front());
        tasks_.pop_front();
    }
    return task;
}
// ctor
ThreadPool::ThreadPool(int size) : _mutex(), _notEmpty()
{
    _threadSize = size;
    threads_.reserve(_threadSize);
    Start();
}

// dtor
ThreadPool::~ThreadPool()
{
    if (_isRunning) Stop();
}
std::future<int> ThreadPool::Enqueue(std::function<int_func> func, int param)
{
    std::unique_lock<std::mutex> lock(_mutex);
    Task task(std::bind(func, param));
    auto f = task.get_future();
    tasks_.push_back(std::move(task));
    _notEmpty.notify_all();
    return f;
}

void ThreadPool::Stop()
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _notEmpty.notify_all();
        _isRunning = false;
    }
    for (auto& thr : threads_)
    {
        thr->join();
    }
}

void ThreadPool::Run()
{
    while (_isRunning)
    {
        Task task(Take());
        if (task.valid())
        {
            task();
        }
    }
}

int main()
{
    ThreadPool p(4);
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 1000; i++)
    {
        futures.push_back(p.Enqueue(
            [](int life) {
                std::cout << "run task..." << std::endl;
                return life + 1;
            },
            i));
    }

    for (auto& f : futures) f.wait();
    for (auto& f : futures) std::cout << f.get() << std::endl;

    p.Stop();
    return 0;
}