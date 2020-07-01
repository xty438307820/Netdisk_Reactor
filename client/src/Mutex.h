#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "CurrentThread.h"
#include "Noncopyable.h"
#include <assert.h>
#include <pthread.h>

// Use as data member of a class, eg.
//
// class Foo
// {
//  public:
//   int size() const;
//
//  private:
//   mutable MutexLock mutex_;
//   std::vector<int> data_ GUARDED_BY(mutex_);
// };
class MutexLock : Noncopyable
{
 public:
  MutexLock()
    : holder_(0)
  {
    pthread_mutex_init(&mutex_, NULL);
  }

  ~MutexLock()
  {
    assert(holder_ == 0);
    pthread_mutex_destroy(&mutex_);
  }

  // must be called when locked
  bool isLockedByThisThread() const
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const
  {
    assert(isLockedByThisThread());
  }

  void lock()
  {
    pthread_mutex_lock(&mutex_);
    assignHolder();
  }

  void unlock()
  {
    unassignHolder();
    pthread_mutex_unlock(&mutex_);
  }

  pthread_mutex_t* getPthreadMutex()
  {
    return &mutex_;
  }

 private:
  friend class Condition;

  class UnassignGuard : Noncopyable
  {
   public:
    explicit UnassignGuard(MutexLock& owner)
      : owner_(owner)
    {
      owner_.unassignHolder();
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();
    }

   private:
    MutexLock& owner_;
  };

  void unassignHolder()
  {
    holder_ = 0;
  }

  void assignHolder()
  {
    holder_ = CurrentThread::tid();
  }

  pthread_mutex_t mutex_;
  pid_t holder_;
};

class MutexLockGuard : Noncopyable
{
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_;
};

#endif
