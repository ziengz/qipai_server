#include "MessageThreadPool.h"
#include "MessageHandler.h"
#include "Thread/ThreadWorker.h"
#include <memory>
#include <vector>

class MessageThreadWorker : public ThreadWorker {
  public:
    MessageThreadWorker(const ThreadStopFlag::Ptr &flag) : ThreadWorker(flag) {}

    virtual ~MessageThreadWorker() = default;

    void addHandler(const MessageHandler::Ptr &handler) {
        _handlers.push_back(handler);
    }

  protected:
    virtual int oneLoopEx() override {
        // 如果为空等待100毫秒
        if (_handlers.empty())
            return 100;
        bool test = false;
        for (const MessageHandler::Ptr handler : _handlers) {
            if (handler->handle()) {
                test = true;
            }
        }
        // 若本次循环处理了消息，则立刻执行下一次循环，否则休眠10毫秒
        return test ? 0 : 10;
    }

  private:
    std::vector<MessageHandler::Ptr> _handlers;
};

void MessageThreadPool::Start(
    int threadNum, const std::vector<MessageHandler::Ptr> &handlers) {
    if (threadNum > 10)
        threadNum = 10; // 最大10个线程
    int count = static_cast<int>(handlers.size());
    if (threadNum > count)
        threadNum = count;
    else if (threadNum < 1)
        return;
    if (isStart())
        return;

    ThreadStopFlag::Ptr flag = std::make_shared<ThreadStopFlag>();
    std::vector<ThreadWorker::Ptr> workers;
    for (int i = 0; i < threadNum; i++) {
        ThreadWorker::Ptr worker = std::make_shared<MessageThreadWorker>(flag);
        workers.push_back(worker);
    }

    std::vector<ThreadWorker::Ptr>::iterator it = workers.begin();
    for (const MessageHandler::Ptr handler : handlers) {
        ThreadWorker::Ptr &tmp = *it;
        std::shared_ptr<MessageThreadWorker> worker =
            std::dynamic_pointer_cast<MessageThreadWorker>(tmp);
        worker->addHandler(handler);
        handler->setWorker(worker);

        std::weak_ptr<MessageHandler> weakHandler = handler;
        worker->dispatch([weakHandler] {
            MessageHandler::Ptr strong = weakHandler.lock();
            strong->initialize();
        });
        ++it;
        if (it == workers.end()) {
            it = workers.begin();
        }
    }
    ThreadPool::Start(flag, workers);
}