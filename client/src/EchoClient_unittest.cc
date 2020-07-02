#include "TcpClient.h"

#include "EventLoop.h"
#include "InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

class EchoClient : Noncopyable
{
 public:
  EchoClient(EventLoop* loop, const InetAddress& listenAddr, const string& id)
    : loop_(loop),
      client_(loop, listenAddr, "EchoClient"+id)
  {
    client_.setConnectionCallback(
        std::bind(&EchoClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&EchoClient::onMessage, this, _1, _2, _3));
    //client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    printf("%s -> %s is %s \n",conn->localAddress().toIpPort().c_str(),conn->peerAddress().toIpPort().c_str(),(conn->connected() ? "UP" : "DOWN"));

    if (conn->connected())
    {
      ++current;
      if (static_cast<size_t>(current) < clients.size())
      {
        clients[current]->connect();
      }
      printf("*** connected\n");
    }
    conn->send("world\n");
  }

  void onMessage(const TcpConnectionPtr& conn, const string& msg, Timestamp time)
  {
    printf("%s recv %lu bytes at %s\n",conn->name().c_str(),msg.size(),time.toString().c_str());
    printf("%s\n",msg.c_str());
    if (msg == "quit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    else if (msg == "shutdown\n")
    {
      loop_->quit();
    }
    else
    {
      conn->send(msg);
    }
  }

  EventLoop* loop_;
  TcpClient client_;
};

int main(int argc, char* argv[])
{
  printf("pid = %d, tid = %d\n",getpid(),CurrentThread::tid());
  signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
  if (argc > 1)
  {
    EventLoop loop;
    InetAddress serverAddr(argv[1], 2000);

    int n = 1;
    if (argc > 2)
    {
      n = atoi(argv[2]);
    }

    clients.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "%d", i+1);
      clients.emplace_back(new EchoClient(&loop, serverAddr, buf));
    }

    clients[current]->connect();
    loop.loop();
  }
  else
  {
    printf("Usage: %s host_ip [current#]\n", argv[0]);
  }
}

