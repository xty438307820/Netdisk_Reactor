#include "EventLoop.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

EventLoop* g_loop;

int main()
{
  printf("main(): pid = %d, tid = %d\n", getpid(), CurrentThread::tid());

  EventLoop loop;

  loop.loop();
}
