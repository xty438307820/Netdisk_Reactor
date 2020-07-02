#include "TcpServer.h"

#include "Thread.h"
#include "EventLoop.h"
#include "InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int numThreads = 3;

using namespace std::placeholders;

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer")
  {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    printf("%s -> %s is %s\n",conn->peerAddress().toIpPort().c_str(),conn->localAddress().toIpPort().c_str(),(conn->connected() ? "UP" : "DOWN"));
    printf("%s\n",conn->getTcpInfoString().c_str());

    conn->send("hello\n");
  }

  void onMessage(const TcpConnectionPtr& conn, const string& msg, Timestamp time)
  {
    printf("%s recv %lu bytes at %s\n",conn->name().c_str(),msg.size(),time.toString().c_str());
    printf("%s\n",msg.c_str());
    if (msg == "exit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    if (msg == "quit\n")
    {
      loop_->quit();
    }
    conn->send(msg);
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main(int argc, char* argv[])
{
  printf("pid = %d, tid = %d\n",getpid(),CurrentThread::tid());
  printf("sizeof TcpConnection = %lu\n",sizeof(TcpConnection));
  signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(2000, false);
  EchoServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

