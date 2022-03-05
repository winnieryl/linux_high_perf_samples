#include <deque>
#include <future>
#include <thread>
#include <functional>
#include <vector>
#include <condition_variable>
using int_func = int(int);
using Task = std::packaged_task<int()>;

class ThreadPool
{
private:
    int _threadSize;
    bool _isRunning;
    std::mutex _mutex;
    std::deque<Task> tasks_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::condition_variable _notEmpty;

    void Start();

    Task Take();

public:
    // ctor
    ThreadPool(int size);
    // dtor
    ~ThreadPool();
    std::future<int> Enqueue(std::function<int_func> func, int param);

    void Stop();

    void Run();
};