#ifndef __COUNTDOWNLATCH_H__
#define __COUNTDOWNLATCH_H__

#include "Condition.h"
#include "Mutex.h"

class CountDownLatch : Noncopyable
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  Condition condition_;
  int count_;
};

#endif
