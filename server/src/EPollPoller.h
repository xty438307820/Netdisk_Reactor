#ifndef __EPOLLPOLLER_H__
#define __EPOLLPOLLER_H__

#include "EventLoop.h"

#include <vector>
#include <map>

struct epoll_event;

class EPollPoller
{
 public:
  typedef std::vector<Channel*> ChannelList;

  EPollPoller(EventLoop* loop);
  ~EPollPoller();

  Timestamp poll(int timeoutMs, ChannelList* activeChannels);
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);

  void assertInLoopThread() const
  {
    ownerLoop_->assertInLoopThread();
  }

  bool hasChannel(Channel* channel) const;

 private:
  static const int kInitEventListSize = 16;

  static const char* operationToString(int op);

  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;
  void update(int operation, Channel* channel);

  typedef std::vector<struct epoll_event> EventList;

  int epollfd_;
  EventList events_;
  
  protected:
    typedef std::map<int, Channel*> ChannelMap; //<fd,Channel*>
    ChannelMap channels_;

  private:
    EventLoop* ownerLoop_;
};

#endif
