#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#include "TcpConnection.h"

#include <map>
#include <atomic>

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

//支持reactor和reactors模式
class TcpServer : Noncopyable
{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback;
  typedef std::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;
  typedef std::function<void (const TcpConnectionPtr&,
                              const string&,
                              Timestamp)> MessageCallback;

  enum Option
  {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg,
            Option option = kNoReusePort);
  ~TcpServer();

  const string& ipPort() const { return ipPort_; }
  const string& name() const { return name_; }
  EventLoop* getLoop() const { return loop_; }
 
  // start之前调用
  // 设置子线程数目
  // 0 - 不会创建线程,accpet及所有操作在主线程完成
  // N - 创建子线程,accept在主线程,tcp连接分配给子线程处理
  // 采用轮询的方式给子线程分配TcpConnection
  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }
  
  std::shared_ptr<EventLoopThreadPool> threadPool()
  { return threadPool_; }

  void start();

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd, const InetAddress& peerAddr);
  /// Thread safe.
  void removeConnection(const TcpConnectionPtr& conn);
  /// Not thread safe, but in loop
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  typedef std::map<string, TcpConnectionPtr> ConnectionMap;

  EventLoop* loop_;  //基本的事件循环,用于接受连接
  const string ipPort_;
  const string name_;
  std::unique_ptr<Acceptor> acceptor_;
  std::shared_ptr<EventLoopThreadPool> threadPool_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  ThreadInitCallback threadInitCallback_;
  
  std::atomic<int32_t> started_;
  
  int nextConnId_;
  ConnectionMap connections_;
};

#endif
