#ifndef __EVENTLOOP_H__
#define __EVENTLOOP_H__

#include <atomic>
#include <functional>
#include <vector>
#include <memory>

#include "Mutex.h"
#include "CurrentThread.h"
#include "Timestamp.h"
#include "Noncopyable.h"

class Channel;
class EPollPoller;
class TimerQueue;

class EventLoop : Noncopyable
{
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();

  void loop();

  void quit();

  Timestamp pollReturnTime() const { return pollReturnTime_; }

  int64_t iteration() const { return iteration_; }

  void runInLoop(Functor cb);

  void queueInLoop(Functor cb);

  size_t queueSize() const;

  void wakeup();
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  
  bool eventHandling() const { return eventHandling_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();  // waked up
  void doPendingFunctors();

  void printActiveChannels() const;

  typedef std::vector<Channel*> ChannelList;

  bool looping_; /* only used in current thread */
  std::atomic<bool> quit_;
  bool eventHandling_; /* only used in current thread */
  bool callingPendingFunctors_; /* only used in current thread */
  int64_t iteration_;
  const pid_t threadId_;
  Timestamp pollReturnTime_;
  std::unique_ptr<EPollPoller> poller_;
  
  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannel_;

  ChannelList activeChannels_;
  Channel* currentActiveChannel_;

  mutable MutexLock mutex_;
  std::vector<Functor> pendingFunctors_;
};

#endif
