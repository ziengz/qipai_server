#include <condition_variable>
#include <mutex>

class ThreadBlocker{
private:
    std::condition_variable _cond;
    bool _flag;
    std::mutex _mtx;

public:
    ThreadBlocker();
    virtual ~ThreadBlocker();

    typedef std::shared_ptr<ThreadBlocker> Ptr;

    /**
     * 阻塞线程
     */
    void block();

    /**
     * 通知激活
     */
    void singal();

    void reset();

};