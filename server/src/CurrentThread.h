#ifndef __CURRENTTHREAD_H__
#define __CURRENTTHREAD_H__

#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread
{
  // internal
  extern __thread int t_cachedTid;
  extern __thread char t_tidString[32];
  extern __thread int t_tidStringLength;
  extern __thread const char* t_threadName;

  inline int tid()
  {
    if (t_cachedTid == 0)
    {
      t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
      t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    }
    return t_cachedTid;
  }

  inline const char* tidString() // for logging
  {
    return t_tidString;
  }

  inline int tidStringLength() // for logging
  {
    return t_tidStringLength;
  }

  inline const char* name()
  {
    return t_threadName;
  }

  bool isMainThread();
}

#endif 
