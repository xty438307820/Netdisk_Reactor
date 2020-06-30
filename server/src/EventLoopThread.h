#ifndef __EVENTLOOPTHREAD_H__
#define __EVENTLOOPTHREAD_H__

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"

class EventLoop;

class EventLoopThread : Noncopyable
{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const std::string& name = std::string());
  ~EventLoopThread();
  EventLoop* startLoop();

 private:
  void threadFunc();

  EventLoop* loop_;
  bool exiting_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;
  ThreadInitCallback callback_;
};

#endif

