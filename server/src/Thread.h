#ifndef __THREAD_H__
#define __THREAD_H__

#include "CountDownLatch.h"

#include <functional>
#include <memory>
#include <pthread.h>
#include <atomic>

class Thread : Noncopyable
{
 public:
  typedef std::function<void ()> ThreadFunc;

  explicit Thread(ThreadFunc, const std::string& name = std::string());
  // FIXME: make it movable in C++11
  ~Thread();

  void start();
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const std::string& name() const { return name_; }

  static int numCreated() { return numCreated_.load(std::memory_order_relaxed); }

 private:
  void setDefaultName();

  bool       started_;
  bool       joined_;
  pthread_t  pthreadId_;
  pid_t      tid_;
  ThreadFunc func_;
  std::string     name_;
  CountDownLatch latch_;

  static std::atomic<int32_t> numCreated_;
};

#endif
